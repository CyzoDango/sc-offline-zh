// Included once by spawner.cpp, after the seat engine. All engine work stays on
// ProcessShipMenu's game-thread tick; the UI only queues requests or reads copies.
struct ShipBatch {
    bool active=false,item=false;
    char nativeClass[96]{},spawnClass[96]{};
    MenuSpawnOptions options;
    uint64_t zone=0,first=0,last=0;
    double origin[3]{},rotation[4]{0,0,0,1};
    double basis[3][3]{};
    int done=0;
    double spacing=0;
    DWORD next=0,sinceLast=0;
};
static ShipBatch g_shipBatch;
static char g_spawnItems[5000][96];
static char g_spawnItemCats[5000][48];
static volatile LONG g_spawnItemCount=-1,g_wantSpawnItems=0;
static struct {bool pending=false;int index=0,count=1;float distance=3;} g_itemRequest;

bool EntityProtectedFromRemoval(uint64_t id){
    if(!id)return true;
    __try {
        const uint64_t player=LocalPlayerEntityId();
        uintptr_t actor=0,entity=0;
        if(!player||!GetLocalPlayer(actor,entity)||id==player)return true;
        int depth=0;
        for(uintptr_t z=VCall<uintptr_t>(entity,0x6B8);z&&depth<64;z=ZoneParent(z),++depth)
            if(ZoneId(z)==id)return true;
        return depth==64;
    }__except(EXCEPTION_EXECUTE_HANDLER){return true;}
}

static bool CaptureShipFrame(ShipBatch& b){
    __try {
        uintptr_t actor=0,entity=0;
        if(!GetLocalPlayer(actor,entity))return false;
        uintptr_t zone=VCall<uintptr_t>(entity,0x6B8),outside=zone;
        if(!zone)return false;
        // Escape any containing ship grid, so the new ships don't become children
        // of the ship the player is aboard. Keep the same planet/system frame.
        int depth=0;
        for(uintptr_t z=zone;z&&depth<64;z=ZoneParent(z),++depth){
            const uintptr_t e=EntityByIdSafe(ZoneId(z));
            if(e&&EntityComponent(e,"IItemPortContainer"))outside=ZoneParent(z);
        }
        if(depth==64||!outside)return false;
        b.zone=ZoneId(outside);
        if(!b.zone||ZoneFromId(b.zone)!=outside)return false;
        double local[3],world[3],q[4];
        Vec3Out(entity,0x2B8,local);
        if(!EntityRotation(entity,q))return false;
        double norm=0;for(double v:q)norm+=v*v;
        if(!std::isfinite(norm)||norm<0.9||norm>1.1)return false;
        for(double&v:q)v/=std::sqrt(norm);
        LocalToWorld(zone,local,world);
        if(!WorldToLocal(outside,world,b.origin))return false;
        sco_zh::FleetVec right,forward,up;sco_zh::ShipAxes(q,right,forward,up);
        const double* axes[]={right.data(),forward.data(),up.data()};
        for(int a=0;a<3;++a){
            double p[3],w[3],out[3];
            for(int k=0;k<3;++k)p[k]=local[k]+100000*axes[a][k];
            LocalToWorld(zone,p,w);if(!WorldToLocal(outside,w,out))return false;
            double len=0;for(int k=0;k<3;++k){b.basis[a][k]=(out[k]-b.origin[k])/100000;len+=b.basis[a][k]*b.basis[a][k];}
            if(!std::isfinite(len)||std::fabs(len-1)>1e-4)return false;
        }
        // The engine accepts identity in the external zone. Placement follows the
        // captured player axes; ship orientation stays level in that external grid.
        b.rotation[0]=b.rotation[1]=b.rotation[2]=0;b.rotation[3]=1;
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool BatchZoneAlive(uint64_t id){__try{return ZoneFromId(id)!=0;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}

int Menu_GetSpawnedShips(MenuSpawnedShip* out,int max,bool items){
    AcquireSRWLockShared(&g_menuLock);int n=0;
    for(int i=0;i<g_fleetViewCount&&n<max;++i)if(g_fleetView[i].item==items)out[n++]=g_fleetView[i];
    ReleaseSRWLockShared(&g_menuLock);return n;
}
void Menu_RequestDeleteShip(unsigned long long id){AcquireSRWLockExclusive(&g_menuLock);g_deleteShipRequest=id;ReleaseSRWLockExclusive(&g_menuLock);}
void Menu_RequestDeleteAllShips(bool items){AcquireSRWLockExclusive(&g_menuLock);g_deleteAllRequest=items?2:1;g_cancelBatchRequest=true;ReleaseSRWLockExclusive(&g_menuLock);}
void Menu_RequestCancelBatch(){AcquireSRWLockExclusive(&g_menuLock);g_cancelBatchRequest=true;ReleaseSRWLockExclusive(&g_menuLock);}
bool Menu_SpawnBatchBusy(int* done,int* total){
    AcquireSRWLockShared(&g_menuLock);if(done)*done=g_batchDone;if(total)*total=g_batchTotal;const bool busy=g_batchBusy;ReleaseSRWLockShared(&g_menuLock);return busy;
}
bool Menu_FleetRecipeAvailable(int index,int relation,int crew){
    if(index<0||index>=g_menuShipCount)return false;
    if(relation==0&&crew==-1)return true;
    return relation>=0&&relation<4&&crew>=0&&crew<2&&(g_recipeReady[index]&(1u<<(relation*2+crew)));
}
float Menu_ShipSpacing(int index,float extra){
    if(index<0||index>=g_menuShipCount)return 1500;
    const auto&s=g_menuShips[index];return static_cast<float>(sco_zh::ShipSpacing(s.width,s.length,s.height,extra));
}
int Menu_SpawnItemCount(){if(g_spawnItemCount<0)InterlockedExchange(&g_wantSpawnItems,1);return g_spawnItemCount;}
const char* Menu_SpawnItemName(int i){return i>=0&&i<g_spawnItemCount?g_spawnItems[i]:"";}
const char* Menu_SpawnItemCategory(int i){return i>=0&&i<g_spawnItemCount?g_spawnItemCats[i]:"";}
void Menu_RequestSpawnItem(int index,int count,float distance){AcquireSRWLockExclusive(&g_menuLock);g_itemRequest={true,index,count,distance};ReleaseSRWLockExclusive(&g_menuLock);}
static int BuildSpawnItems(){
    char path[MAX_PATH];if(!DataFilePath(path,sizeof(path),"item_catalog.txt"))return 0;
    FILE*f=_fsopen(path,"r",_SH_DENYNO);if(!f)return 0;
    int n=0;char line[256];
    while(n<5000&&fgets(line,sizeof(line),f)){
        line[strcspn(line,"\r\n#")]=0;char* tab=strchr(line,'\t');if(!tab||tab-line>=96)continue;
        *tab++=0;if(!ClassInRegistry(line))continue;
        strcpy_s(g_spawnItems[n],line);strncpy_s(g_spawnItemCats[n],tab,_TRUNCATE);++n;
    }
    fclose(f);return n;
}
static void PublishFleet(){
    AcquireSRWLockExclusive(&g_menuLock);
    g_fleetViewCount=g_fleetCount;
    for(int i=0;i<g_fleetCount;++i)g_fleetView[i]=g_fleet[i].view;
    g_batchBusy=g_shipBatch.active;g_batchDone=g_shipBatch.done;g_batchTotal=g_shipBatch.options.count;
    ReleaseSRWLockExclusive(&g_menuLock);
}
static bool FleetHasRoom(int count){int active=0;for(int i=0;i<g_fleetCount;++i)if(g_fleet[i].view.state!=3)++active;return active+count<=512;}
static bool StartShipBatch(int index,const MenuSpawnOptions& opt,DWORD now){
    if(g_shipBatch.active){SetMenuStatus("已有生成队列，请等待完成或先取消。");return false;}
    if(index<0||index>=g_menuShipCount||opt.count<1||opt.count>20||opt.direction<0||opt.direction>5
       ||!std::isfinite(opt.height)||opt.height<0||opt.height>10000||!std::isfinite(opt.extraGap)||opt.extraGap<0||opt.extraGap>5000){SetMenuStatus("生成参数无效。");return false;}
    if(!FleetHasRoom(opt.count)){SetMenuStatus("生成记录已满，请先删除部分本菜单生成的物品或船只。");return false;}
    if(!Menu_FleetRecipeAvailable(index,opt.relation,opt.crew)){SetMenuStatus("这个版本的独立阵营／船员预设未加载；请换版本或选“沿用模板”。");return false;}
    ShipBatch b;b.options=opt;b.next=now;b.spacing=Menu_ShipSpacing(index,opt.extraGap);
    strncpy_s(b.nativeClass,g_menuShips[index].name,_TRUNCATE);
    const std::string cls=sco_zh::FleetRecipe(b.nativeClass,opt.relation,opt.crew);
    strncpy_s(b.spawnClass,cls.c_str(),_TRUNCATE);
    if(!CaptureShipFrame(b)){SetMenuStatus("暂时读不到人物位置或朝向，请进入场景后重试。");return false;}
    b.active=true;g_shipBatch=b;
    SetMenuStatus("生成队列已准备：%d 艘，中心间距 %.0f 米。",opt.count,b.spacing);return true;
}
static void StartItemBatch(int index,int count,float distance,DWORD now){
    if(g_shipBatch.active){SetMenuStatus("已有生成队列，请先等待或取消。");return;}
    if(index<0||index>=g_spawnItemCount||count<1||count>20||!std::isfinite(distance)||distance<1||distance>100)return;
    if(!FleetHasRoom(count)){SetMenuStatus("生成记录已满。");return;}
    ShipBatch b;b.item=true;b.options.count=count;b.options.direction=4;b.options.height=distance;b.options.seatMode=SeatMode_None;b.options.flightReady=false;b.next=now;b.spacing=1.5;
    strncpy_s(b.nativeClass,g_spawnItems[index],_TRUNCATE);strcpy_s(b.spawnClass,b.nativeClass);
    if(!CaptureShipFrame(b)){SetMenuStatus("读不到人物位置或朝向，请进入场景后重试。");return;}
    b.active=true;g_shipBatch=b;
}
static void DeleteTrackedShip(FleetEntry& e,DWORD now){
    if(e.view.state==3||e.view.state==2)return;
    if(EntityProtectedFromRemoval(e.view.id)){SetMenuStatus("已保留你所在的船（或当前人物状态尚不可读）。");return;}
    if(!CanRemoveEntities()){SetMenuStatus("当前游戏的删除接口不可用，保留生成记录。");return;}
    if(!e.view.item){
        if(g_seatJob.id==e.view.id)g_seatJob.id=0;
        if(g_powerJob.shipId==e.view.id)g_powerJob.shipId=0;
        for(int i=0;i<g_crewJobCount;)if(g_crewJobs[i].shipId==e.view.id){RemoveEntityById(g_crewJobs[i].npcId);g_crewJobs[i]=g_crewJobs[--g_crewJobCount];}else ++i;
        const int seats=EnumerateShipSeats(e.view.id);
        SeatInfo npcs[kMaxSeats];int n=0;
        for(int i=0;i<seats;++i)if(g_seatList[i].state==SeatState_Npc)npcs[n++]=g_seatList[i];
        for(int i=0;i<n;++i)EvictSeat(npcs[i]);
        if(g_crewFill.ship==e.view.id)g_crewFill.active=false;
        if(g_target.shipId==e.view.id)SetTarget(0,"",true);
    }
    e.view.state=2;e.removedAt=now;
    if(EntityAlive(e.view.id)){RemoveEntityById(e.view.id);e.removeSent=true;}
    Log("[fleet] requested removal %llu (%s)",e.view.id,e.view.cls);
}
static void ProcessFleet(DWORD now){
    uint64_t remove=0;int all=0;bool cancel=false;
    AcquireSRWLockExclusive(&g_menuLock);remove=g_deleteShipRequest;g_deleteShipRequest=0;all=g_deleteAllRequest;g_deleteAllRequest=0;cancel=g_cancelBatchRequest;g_cancelBatchRequest=false;
    const auto item=g_itemRequest;g_itemRequest.pending=false;ReleaseSRWLockExclusive(&g_menuLock);
    if(cancel){g_shipBatch.active=false;SetMenuStatus("已停止后续生成，已生成的对象仍保留。");}
    if(item.pending)StartItemBatch(item.index,item.count,item.distance,now);
    if(all||remove)for(int i=0;i<g_fleetCount;++i)if((all&&g_fleet[i].view.item==(all==2))||g_fleet[i].view.id==remove)DeleteTrackedShip(g_fleet[i],now);
    static DWORD lastPoll=0;
    if(now-lastPoll>=1000){
        lastPoll=now;
        for(int i=0;i<g_fleetCount;++i){
            auto&e=g_fleet[i];if(e.view.state==3)continue;
            const bool alive=EntityAlive(e.view.id);
            if(alive)e.seen=true;
            if(e.view.state==2){
                if(EntityProtectedFromRemoval(e.view.id)){e.view.state=alive?1:4;e.removeSent=false;continue;}
                if(alive&&!e.removeSent){RemoveEntityById(e.view.id);e.removeSent=true;}
                if(!alive&&e.seen&&now-e.removedAt>3000)e.view.state=3;
            }else e.view.state=alive?1:(now-e.born>90000?4:0);
        }
    }
    auto&b=g_shipBatch;
    if(b.active&&static_cast<LONG>(now-b.next)>=0){
        if(!BatchZoneAlive(b.zone)){b.active=false;SetMenuStatus("生成队列已停止：原来的区域已卸载。");}
        else if(b.last&&!EntityAlive(b.last)){
            if(now-b.sinceLast>90000){b.active=false;SetMenuStatus("上一个对象长时间没有载入，已停止后续生成。");}else b.next=now+500;
        }else{
            const double identity[4]={0,0,0,1};
            const auto offset=sco_zh::ShipOffset(b.options.direction,b.options.height,b.done,b.options.count,b.spacing,identity);
            double pos[3];for(int k=0;k<3;++k)pos[k]=b.origin[k]+offset[0]*b.basis[0][k]+offset[1]*b.basis[1][k]+offset[2]*b.basis[2][k];
            uint64_t id=0;
            if(const char* err=SpawnEntityInZone(b.spawnClass,b.zone,pos,b.rotation,id)){b.active=false;SetMenuStatus("生成失败，已停止队列：%s",err);}
            else{
                TrackFleet(id,b.nativeClass,b.options.relation,b.options.crew,b.item);
                if(!b.first)b.first=id;b.last=id;b.sinceLast=now;++b.done;
                b.next=now+(b.item?250:b.spacing>200?3000:1000);
                Log("[fleet] %d/%d %s -> %s id=%llu relation=%d crew=%d zone=%llu pos=%.2f,%.2f,%.2f",b.done,b.options.count,b.nativeClass,b.spawnClass,id,b.options.relation,b.options.crew,b.zone,pos[0],pos[1],pos[2]);
                if(b.done>=b.options.count){
                    b.active=false;
                    if(!b.item){
                        SetTarget(b.first,b.nativeClass);
                        if(b.options.seatMode==SeatMode_Pilot||b.options.seatMode==SeatMode_Named)StartSeatJob(b.first,b.nativeClass,b.options.seatMode,b.options.seatName,b.options.replaceNpc,b.options.flightReady,0,now);
                    }
                    SetMenuStatus("已请求生成 %d %s；大型飞船载入仍需要时间。",b.done,b.item?"件物品":"艘飞船");
                }
            }
        }
    }
    if(g_spawnItemCount<0&&g_wantSpawnItems&&LocalPlayerEntityId()){const int n=BuildSpawnItems();InterlockedExchange(&g_spawnItemCount,n);}
    PublishFleet();
}
