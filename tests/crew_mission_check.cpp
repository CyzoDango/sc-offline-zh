#include "../src/zh/seat_policy.h"
#include "../src/zh/mission_policy.h"
#include <cstdio>
int main(){
    int checks=0,failed=0;auto expect=[&](bool ok,const char*why){++checks;if(!ok){++failed;printf("FAIL %s\n",why);}};
    using namespace sco_zh;
    expect(NamedPilot("RSI_Bengal_SCItem_Seat_Pilot_200009392239"),"Bengal pilot identified");
    expect(NamedPilot("AEGS_Javelin_SCItem_Seat_Pilot_100"),"Javelin pilot identified");
    expect(!NamedPilot("RSI_Bengal_SCItem_Seat_CoPilot_100"),"copilot is not the primary pilot");
    expect(!NamedPilot("SCItem_Seat_Generic_3"),"generic seat is not a named pilot");
    expect(!NamedPilot("Seat_Co_Pilot"),"separated copilot is not primary");
    expect(!NamedPilot("Seat_Co-Pilot"),"hyphenated copilot is not primary");
    expect(!NamedPilot("Seat_Secondary_Pilot"),"secondary pilot is not primary");
    expect(!NamedPilot("Seat_Auxiliary_Pilot"),"auxiliary pilot is not primary");
    expect(PlayerOwnsPilotSeat("Seat_Pilot",1,42,42),"actual pilot occupancy accepted");
    expect(!PlayerOwnsPilotSeat("Seat_Co_Pilot",1,42,42),"actual copilot occupancy rejected");
    expect(!PlayerOwnsPilotSeat("Seat_Pilot",1,43,42),"NPC occupancy rejected");
    expect(!PlayerOwnsPilotSeat("Seat_Pilot",1,0,0),"missing player identity rejected");
    expect(!PlayerOwnsPilotSeat("Seat_Pilot",0,42,42),"locked seat rejected");
    struct Seat{const char* name;unsigned priority;int usable;};
    Seat seats[]={{"Seat_Generic",9999,1},{"Seat_Co_Pilot",2000,1},{"Seat_Pilot",1000,1},{"Seat_Pilot",900,1}};
    expect(PrimaryPilotIndex(seats,4)==2,"named primary outranks generic and secondary seats");
    seats[3].priority=1000;expect(PrimaryPilotIndex(seats,4)==-1,"ambiguous main seats rejected");
    seats[3].usable=0;expect(PrimaryPilotIndex(seats,4)==2,"locked competing primary excluded");
    expect(PrimaryPilotIndex(seats,2)==-1,"no primary cannot be inferred from priority");
    expect(SamePilotLease(true,10,20,10,20),"same ship and main seat retains override");
    expect(!SamePilotLease(false,10,20,10,20),"standing up revokes override");
    expect(!SamePilotLease(true,11,20,10,20),"ship change revokes override");
    expect(!SamePilotLease(true,10,21,10,20),"seat change revokes override");
    expect(SeatRank("Seat_Pilot",1000)>SeatRank("Seat_CoPilot",1000),"equal-priority seats prefer real pilot");
    expect(SeatRank("Seat_Pilot",0)>SeatRank("Seat_Generic_3",1000),"named pilot beats mislabeled generic priority");
    expect(MissionCategory("HaulCargo_Stanton")==1,"hauling classified");
    expect(MissionCategory("FoxwellEnforcement_ShipAmbush_Stanton")==2,"ship combat classified");
    expect(MissionCategory("BountyHuntersGuild_Easy")==2,"bounty classified");
    expect(MissionCategory("CFP_EliminateAll_Outposts")==3,"ground combat classified");
    expect(MissionCategory("HWS_KillAnimals")==3,"animal objectives classified");
    expect(MissionCategory("MissingPerson")==4,"search classified");
    expect(MissionSystemMatches("CFP_EliminateAll_Pyro","PYRO"),"system matching ignores case");
    expect(!MissionSystemMatches("CFP_EliminateAll_Pyro","Stanton"),"Pyro mission excluded in Stanton");
    expect(!MissionSystemMatches("Contract_Nyx","Pyro"),"Nyx mission excluded in Pyro");
    expect(MissionSystemMatches("Contract_Global","Nyx"),"unspecified system retained");
    printf("crew and mission policy: %d checks, %d failures; no game executed\n",checks,failed);return failed?1:0;
}
