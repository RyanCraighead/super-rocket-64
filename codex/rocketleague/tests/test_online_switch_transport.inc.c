/* Real writer/ingress/apply regression. The lifecycle fixture separately proves
 * that every committed online selection increments the owner's runtime epoch. */
static struct Packet switch_packet(u32 seq,u32 epoch,int kind,float x){
 struct Packet p=sample(1,1,PACKET_DESTINATION_BROADCAST,seq,kind==CNET_OCTANE);
 CharacterNetState s;
 CHECK(character_net_decode(&s,p.buffer+p.dataLength-CNET_WIRE_SIZE,CNET_WIRE_SIZE));
 s.kind=kind;s.epoch=epoch;s.car.position[0]=x;
 CHECK(character_net_encode(p.buffer+p.dataLength-CNET_WIRE_SIZE,CNET_WIRE_SIZE,&s));
 return p;
}
static void test_online_switch_transport(void){
 character_net_clear_all();gNetworkType=NT_SERVER;
 gNetworkPlayerLocal=gNetworkPlayerServer=&gNetworkPlayers[0];
 for(unsigned i=0;i<3;i++){
  gNetworkPlayers[i].connected=true;gNetworkPlayers[i].globalIndex=i;
  gNetworkPlayers[i].currPositionValid=true;
  gNetworkPlayers[i].currLevelSyncValid=gNetworkPlayers[i].currAreaSyncValid=true;
 }
 gMarioStates[1].health=0x880;gMarioStates[1].freeze=0;
 gMarioStates[1].heldObj=gMarioStates[1].heldByObj=gMarioStates[1].riddenObj=NULL;
 packetCaps=verifiedCapFlags[1]=MARIO_WING_CAP;
 unsigned oldCoinClears=coinClearCalls[1];
 CharacterNetState state;RocketSnapshot car;uint32_t before,after;
 struct Packet p=switch_packet(100,7,CNET_OCTANE,-400);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&before));
 CHECK(character_net_remote_update(&gMarioStates[1]));
 CHECK(objects[1].oIntangibleTimer==-1&&(objects[1].header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
 struct Packet oldCar=switch_packet(101,7,CNET_OCTANE,-300);
 p=switch_packet(102,8,CNET_MARIO,0);packet_receive(&p);
 CHECK(!character_net_is_car(1)&&!character_net_interaction_state(1,&state,NULL));
 CHECK(!character_net_remote_update(&gMarioStates[1]));
 CHECK(objects[1].oIntangibleTimer==0&&!(objects[1].header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
 CHECK(!character_net_snapshot(1,&car));
 unsigned drawsBefore=drawCalls;character_net_draw(NULL,NULL,NULL);CHECK(drawCalls==drawsBefore);
 int acceptedBefore=accepted;packet_receive(&oldCar);
 CHECK(accepted==acceptedBefore&&!character_net_is_car(1)&&objects[1].oIntangibleTimer==0);
 p=switch_packet(104,9,CNET_OCTANE,400);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&after)&&after!=before);
 CHECK(character_net_snapshot(1,&car)&&car.position[0]==400); // No Mario/car interpolation.
 struct Packet delayedMario=switch_packet(103,8,CNET_MARIO,0);acceptedBefore=accepted;
 packet_receive(&delayedMario);CHECK(accepted==acceptedBefore&&character_net_is_car(1));
 CHECK(verifiedCapFlags[1]==MARIO_WING_CAP&&coinClearCalls[1]==oldCoinClears);

 /* Both transitions can arrive between two contact boundaries. Even if a
  * sender repeats an epoch, the observed kind changes invalidate the sweep. */
 before=after;p=switch_packet(105,9,CNET_MARIO,0);packet_receive(&p);
 p=switch_packet(106,9,CNET_OCTANE,-400);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&after)&&after!=before);
 CHECK(character_net_snapshot(1,&car)&&car.position[0]==-400);
 /* A lost Mario packet is recovered by the newer committed epoch on Octane.
  * Reusing both kind and epoch across an unseen transition is not detectable. */
 before=after;p=switch_packet(108,11,CNET_OCTANE,400);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&after)&&after!=before);
 CHECK(character_net_snapshot(1,&car)&&car.position[0]==400);
 before=after;p=switch_packet(109,11,CNET_OCTANE,420);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&after)&&after==before);
 /* A rejected delayed identity change must not increment the generation. */
 delayedMario=switch_packet(107,10,CNET_MARIO,0);packet_receive(&delayedMario);
 CHECK(character_net_interaction_state(1,&state,&after)&&after==before);
 double savedTime=fixtureNow;fixtureNow+=.201;
 CHECK(!character_net_interaction_state(1,&state,NULL));fixtureNow=savedTime;
 gMarioStates[1].health=0xff;CHECK(!character_net_interaction_state(1,&state,NULL));gMarioStates[1].health=0x880;
 gMarioStates[1].heldByObj=&objects[2];CHECK(!character_net_interaction_state(1,&state,NULL));gMarioStates[1].heldByObj=NULL;
 gNetworkPlayers[1].currLevelAreaSeqId++;CHECK(!character_net_interaction_state(1,&state,NULL));gNetworkPlayers[1].currLevelAreaSeqId--;
 /* Native hidden actions remain native: never force visibility on a Mario
  * transition just to undo Octane's hidden actor. */
 p=switch_packet(110,12,CNET_MARIO,0);
 struct PacketPlayerData native;size_t at=p.dataLength-CNET_WIRE_SIZE-sizeof native;
 memcpy(&native,p.buffer+at,sizeof native);native.action=ACT_BUBBLED;native.nodeFlags=GRAPH_RENDER_INVISIBLE;
 memcpy(p.buffer+at,&native,sizeof native);packet_receive(&p);
 CHECK(!character_net_remote_update(&gMarioStates[1]));
 CHECK(objects[1].header.gfx.node.flags&GRAPH_RENDER_INVISIBLE);
 CHECK(verifiedCapFlags[1]==MARIO_WING_CAP&&coinClearCalls[1]==oldCoinClears);
 character_net_clear(1);CHECK(coinClearCalls[1]==oldCoinClears+1&&!verifiedCapFlags[1]);
 CHECK(!character_net_interaction_state(1,&state,NULL));
 p=switch_packet(1,11,CNET_OCTANE,420);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&after)&&after!=before); // Reconnect restarts sequence only.

 /* The writer follows committed selection even when opposite to CLI launch
  * mode, preserves monotonically increasing sequence, and sends promptly at
  * the ordinary native player boundary without waiting for input changes. */
 switchEnabled=1;selectedCharacter=CHARACTER_OCTANE;gCLIOpts.rocketCar=false;
 sourceEpoch=20;sourceSnapshot=&car;presentationSnapshot=NULL;
 car.basis[0]=car.basis[4]=car.basis[8]=1;
 struct Packet out={0};CHECK(character_net_write(&out));
 CHECK(character_net_decode(&state,out.buffer,CNET_WIRE_SIZE));
 CHECK(state.kind==CNET_OCTANE&&state.active==CNET_DRIVING&&state.epoch==20);
 u32 lastSeq=state.sequence;
 selectedCharacter=CHARACTER_MARIO;gCLIOpts.rocketCar=true;sourceEpoch++;
 memset(&out,0,sizeof out);CHECK(character_net_write(&out));
 CHECK(character_net_decode(&state,out.buffer,CNET_WIRE_SIZE));
 CHECK(state.kind==CNET_MARIO&&!state.active&&!state.interaction&&state.sequence>lastSeq);
 gMarioStates[0].action=ACT_IDLE;memset(&controllers[0],0,sizeof controllers[0]);
 capturedPacket=&out;network_update_player();out.dataLength=0;
 network_update_player();CHECK(!out.dataLength); // Stable player keeps ordinary cadence.
 selectedCharacter=CHARACTER_OCTANE;sourceEpoch++;
 network_update_player();CHECK(out.dataLength>CNET_WIRE_SIZE);
 CHECK(character_net_decode(&state,out.buffer+out.dataLength-CNET_WIRE_SIZE,CNET_WIRE_SIZE));
 CHECK(state.kind==CNET_OCTANE&&state.epoch==sourceEpoch&&state.sequence>lastSeq);
 out.dataLength=0;sourceEpoch+=2;network_update_player(); // Lost/coalesced departure, same final kind.
 CHECK(out.dataLength>CNET_WIRE_SIZE);
 capturedPacket=NULL;switchEnabled=0;sourceSnapshot=NULL;packetCaps=0;
 puts("online switch transport: selected writer, immediate boundary send, loss/reorder, rapid round trip, native visibility, epoch/generation, preserved leases/ledgers passed");
}
