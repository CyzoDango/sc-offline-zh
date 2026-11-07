#include "multiplayer.h"
#include "tabs.h"
#include "../menu_ui.h"
#include <cstdio>
#include <string>
namespace {
char g_message[192]="";
void Answer(const char*err,const char*ok){snprintf(g_message,sizeof(g_message),"%s",err?err:ok);}
void Field(const char*label){const auto p=ImGui::GetCursorPos();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(p.x+Ui(180));ImGui::SetNextItemWidth(-1);}
void SessionForm(MpSettings&s){
    SectionHeading("身份");Field("显示名称");MTextField("##mpName","其他玩家看到的名字",s.name,sizeof(s.name));
    Field("会话口令");MTextField("##mpPass","至少 8 个字符",s.passphrase,sizeof(s.passphrase),ImGuiInputTextFlags_Password);
    MSetting("在此电脑记住口令","口令以明文保存在本地 multiplayer.db",&s.rememberPass);
    SectionHeading("创建会话");Field("UDP 端口");ImGui::InputInt("##mpHostPort",&s.hostPort,0);
    if(MActionRow("host","创建局域网会话","使用上面的端口与口令","创建"))Answer(Mp_Host(),"会话已创建，把地址、端口和口令发给其他玩家。");
    SectionHeading("加入会话");Field("主机地址");MTextField("##mpAddr","例如 192.168.1.20",s.joinAddress,sizeof(s.joinAddress));Field("UDP 端口");ImGui::InputInt("##mpJoinPort",&s.joinPort,0);
    if(MActionRow("join","加入已有会话","使用上面的地址与口令","加入"))Answer(Mp_Join(),"正在加入……");
}
void PeerList(const char*filter){
    MListHeader("名称","身份 / 飞船");int row=0;for(int i=0;i<Mp_PeerCount();++i){MpPeerView v;if(!Mp_Peer(i,v)||!MatchesFilter(v.name,filter))continue;
        const std::string info=std::string(v.avatar)+" / "+(v.ship[0]?v.ship:"无飞船");if(MActionRow(std::to_string(v.peer).c_str(),v.name,info.c_str(),v.self?nullptr:"传送",false,nullptr,row++,info.c_str()))Answer(Mp_GoTo(v.peer),"已传送。");}
    if(!row)Hint("没有匹配的会话成员。");
}
}
void DrawMultiplayerTab(void*,void*){
    static char filter[96]="";SearchBox("##mpSearch","搜索当前会话成员",filter,sizeof(filter));if(!Mp_Enabled()){Hint("多人功能没开启。检查一下 multiplayer 配置和 sco.net，详情看 mod.log。");return;}
    auto&s=Mp_Settings();SectionHeading("会话状态");MActionRow("state",Mp_StateText(),Mp_InSession()?"已加入会话":"未加入会话",nullptr);
    if(Mp_LastReason()[0])Hint(Mp_LastReason());if(g_message[0])Hint(g_message);
    if(Mp_InSession()){if(MButton("离开会话",true)){Mp_Leave();g_message[0]=0;}PeerList(filter);if(!Mp_GhostsReady())Hint("看不到其他玩家：当前游戏版本缺少所需的生成或传送功能。");}else SessionForm(s);
    SectionHeading("其他玩家影像");Field("NPC 模型");MTextField("##mpAvatar","npcs.txt 中的人类模型",s.avatarClass,sizeof(s.avatarClass));
    MSetting("显示其他玩家飞船","仅显示影像，不共享物理或伤害",&s.showShips);
    if(MActionRow("save","本地多人设置","保存身份与影像选项","保存"))Mp_SaveSettings();
    Hint("只支持局域网和 multiplayer_allow 放行的网络。消息有签名但没加密；任务、物品和物理不同步。");if(Mp_AllowText()[0])Hint(Mp_AllowText());
}
