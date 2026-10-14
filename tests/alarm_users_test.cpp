#include "alarm_user_store.h"
#include <sqlite3.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <unistd.h>
using namespace AlarmUsers;
static int assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while (0)
// Real scrypt for fixtures; production uses the gateway's existing PHC helper.
static std::string derive(const std::string &pin,const std::string &salt) {
    unsigned char bytes[32];
    if (!EVP_PBE_scrypt(pin.data(),pin.size(),reinterpret_cast<const unsigned char*>(salt.data()),salt.size(),1024,8,1,0,bytes,sizeof bytes)) return "";
    std::ostringstream out;
    for(auto b:bytes) out<<std::hex<<std::setw(2)<<std::setfill('0')<<int(b);
    return out.str();
}
static std::string hash(const std::string &pin) {
    unsigned char salt[8]; CHECK(RAND_bytes(salt,sizeof salt)==1);
    std::ostringstream s;for(auto c:salt)s<<std::hex<<std::setw(2)<<std::setfill('0')<<int(c);
    return s.str()+":"+derive(pin,s.str());
}
static bool verify(const std::string &value,const std::string &pin) {
    auto i=value.find(':'); return i!=std::string::npos && value.substr(i+1)==derive(pin,value.substr(0,i));
}
static void sql(sqlite3 *db,const std::string &s) { CHECK(sqlite3_exec(db,s.c_str(),nullptr,nullptr,nullptr)==SQLITE_OK); }
static User get(Store &s,int alarm,const std::string &id) {
    std::vector<User> users;CHECK(s.list(alarm,users));
    for(auto u:users)if(u.id==id)return u;
    throw std::runtime_error("missing user");
}
static User add(Store &s,int alarm,const std::string &pin,bool owner=false) {
    User u;u.name=owner?"Owner":"Visitor";u.owner=owner;u.apiArmDisarm=owner;u.allKeypads=owner;
    std::string error;CHECK(s.put(alarm,u,pin,0,error));return u;
}
static void tests() {
    sqlite3 *db=nullptr;CHECK(sqlite3_open(":memory:",&db)==SQLITE_OK);
    sql(db,"CREATE TABLE secrets(uniqueid TEXT PRIMARY KEY,secret TEXT,state INTEGER)");
    Store s(db,verify,hash,[](const std::string &policy,int64_t now){return policy=="window"?(now==0||(now>=100000&&now<200000)?1:0):-1;});
    bool managed=true;CHECK(s.managementEnabled(1,managed)&&!managed);
    std::vector<User> users;CHECK(s.list(1,users)&&users.empty());CHECK(s.managementEnabled(1,managed)&&!managed);
    User denied;denied.name="No owner";std::string error;
    CHECK(!s.put(1,denied,"1111",0,error)&&error=="last_unrestricted_owner_required");
    CHECK(s.list(0,users)&&users.empty());
    auto a=add(s,1,"1357",true);auto b=add(s,2,"2468",true);
    CHECK(a.id!=b.id && a.userRevision==1 && a.revision==1);
    CHECK(s.managementEnabled(1,managed)&&managed);CHECK(s.managementEnabled(3,managed)&&!managed);
    CHECK(!s.erase(1,a.id,a.revision,a.userRevision));
    auto before=a; a.owner=false;CHECK(!s.put(1,a,"",a.revision,error));a=before;
    auto global=get(s,0,a.id);global.enabled=false;CHECK(!s.put(0,global,"",global.userRevision,error));
    CHECK(get(s,1,a.id).enabled);
    auto visitor=add(s,1,"4567");visitor.keypads={{"abc",1},{"def",2}};visitor.remaining=2;visitor.schedule="window";
    CHECK(s.put(1,visitor,"",visitor.revision,error));
    CHECK(!s.restCode(1,"4567",100000));
    CHECK(s.authorize(1,"abc",2,1,0,"4567",100000,true).response==4);
    CHECK(get(s,1,visitor.id).remaining==2);
    CHECK(s.authorize(1,"abc",1,2,0,"4567",99999,true).response==4);
    auto first=s.authorize(1,"abc",1,3,0,"4567",100000,true);
    CHECK(first.ok&&first.response==6&&first.user.id==visitor.id&&first.user.remaining==1);
    auto dup=s.authorize(1,"abc",1,3,0,"4567",100001,true);
    CHECK(dup.ok&&dup.duplicate&&dup.eventId==first.eventId&&get(s,1,visitor.id).remaining==1);
    CHECK(s.authorize(1,"def",2,4,0,"4567",100002,true).response==6);
    CHECK(get(s,1,visitor.id).remaining==0);
    CHECK(s.authorize(1,"abc",1,5,0,"4567",100003,true).response==4);
    // A grant on a different alarm has an independent shared allowance.
    visitor=get(s,0,visitor.id);visitor.remaining=3;visitor.keypads={{"abc",1}};
    CHECK(!s.put(2,visitor,"",0,error)&&error=="current_pin_required");
    CHECK(s.put(2,visitor,"4567",0,error));
    CHECK(get(s,1,visitor.id).remaining==0&&get(s,2,visitor.id).remaining==3);
    CHECK(s.authorize(2,"abc",1,6,0,"4567",100004,true).response==6);
    CHECK(get(s,2,visitor.id).remaining==2&&get(s,1,visitor.id).remaining==0);
    // Equal PINs across disjoint alarms; disabled accounts still reserve them.
    auto c=add(s,3,"9999",true);auto same=add(s,3,"4567");same.enabled=false;
    CHECK(s.put(3,same,"",same.revision,error));
    visitor=get(s,0,visitor.id);
    CHECK(!s.put(3,visitor,"4567",0,error)&&error=="pin_already_assigned");
    CHECK(s.list(3,users)&&users.size()==2);
    global=get(s,0,a.id);
    CHECK(!s.put(0,global,"4567",global.userRevision,error)&&error=="pin_already_assigned");
    CHECK(s.restCode(1,"1357",100000));
    // PIN rotation checks every grant, including destination alarm peers.
    visitor=get(s,0,visitor.id);
    CHECK(!s.put(0,visitor,"2468",visitor.userRevision,error)&&error=="pin_already_assigned");
    CHECK(s.put(0,visitor,"5678",visitor.userRevision,error));
    CHECK(verify(get(s,1,visitor.id).hash,"5678")&&verify(get(s,2,visitor.id).hash,"5678"));
    CHECK(!verify(get(s,1,visitor.id).hash,"4567"));
    auto stale=get(s,2,visitor.id);auto newer=stale;newer.name="Renamed globally";
    CHECK(s.put(0,newer,"",newer.userRevision,error));
    CHECK(!s.put(2,stale,"",stale.revision,error)&&error=="revision_conflict");
    CHECK(get(s,1,visitor.id).name=="Renamed globally");
    // REST-only service user never works at a physical keypad.
    auto service=add(s,1,"6789");service.apiArmDisarm=true;
    CHECK(s.put(1,service,"",service.revision,error));
    for(int mode=0;mode<4;++mode) {
        CHECK(s.authorizeRest(1,"6789",100000,mode).accepted);
        CHECK(s.authorize(1,"abc",1,20+mode,mode,"6789",100010+mode,false).response==4);
    }
    service=get(s,1,service.id);service.arm=false;CHECK(s.put(1,service,"",service.revision,error));
    CHECK(s.authorizeRest(1,"6789",100000,0).accepted&&!s.authorizeRest(1,"6789",100000,1).accepted);
    CHECK(!s.authorizeRest(2,"6789",100000,0).accepted);
    // Owner role can be handed over without special ID/slot treatment.
    auto replacement=add(s,1,"7890");replacement.owner=true;replacement.apiArmDisarm=true;
    CHECK(s.put(1,replacement,"",replacement.revision,error));
    a=get(s,1,a.id);CHECK(s.erase(1,a.id,a.revision,a.userRevision));
    a=get(s,0,a.id);CHECK(s.erase(0,a.id,a.userRevision,a.userRevision));
    CHECK(!s.erase(1,replacement.id,replacement.revision,replacement.userRevision));
    CHECK(!s.erase(0,b.id,b.userRevision,b.userRevision)); // remove grants first
    // Wrong-PIN lockout is independent for every alarm/source/endpoint.
    LockoutPolicy p;p.enabled=true;p.threshold=2;CHECK(s.configureLockout(1,p,0,error));
    auto r=s.authorize(1,"abc",1,30,0,"0000",300000,false);CHECK(r.ok&&!r.locked);
    CHECK(s.authorize(1,"abc",1,30,0,"0000",300001,false).duplicate);
    r=s.authorize(1,"abc",1,31,0,"0000",300002,false);CHECK(r.locked&&r.lockoutLevel==1);
    const auto deadline=r.lockedUntil;
    Store noVerify(db,[](const std::string &,const std::string &)->bool{throw std::runtime_error("lockout evaluated PIN");},hash);
    CHECK(noVerify.authorize(1,"abc",1,32,0,"7890",300003,false).locked);
    CHECK(s.authorize(1,"abc",2,33,0,"0000",300004,false).locked==false);
    CHECK(s.authorize(2,"abc",1,34,0,"2468",300005,false).response==0);
    CHECK(s.authorizeRest(1,"7890",300005).accepted);
    CHECK(s.authorize(1,"abc",1,35,0,"7890",300006,false).lockedUntil==deadline);
    CHECK(s.resetLockout(1));
    // Storage faults never become invalid-PIN decisions/close requests.
    sql(db,"PRAGMA query_only=ON");CHECK(!s.authorize(1,"abc",1,36,0,"0000",300007,false).ok);
    sql(db,"PRAGMA query_only=OFF");
    sql(db,"CREATE TABLE alarm_user_management_v1(alarm INTEGER PRIMARY KEY);INSERT INTO alarm_user_management_v1 VALUES(1)");
    CHECK(!s.managementEnabled(1,managed));CHECK(!s.authorizeRest(1,"7890",300008).ok);
    CHECK(sqlite3_close(db)==SQLITE_OK);
}
static void persistenceAndConcurrency() {
    char path[]="/tmp/global-users-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);close(fd);
    sqlite3 *db=nullptr;CHECK(sqlite3_open(path,&db)==SQLITE_OK);
    sql(db,"CREATE TABLE secrets(uniqueid TEXT PRIMARY KEY,secret TEXT,state INTEGER)");
    Store s(db,verify,hash);add(s,1,"1357",true);
    auto visitor=add(s,1,"2468");visitor.allKeypads=true;visitor.remaining=1;std::string error;
    CHECK(s.put(1,visitor,"",visitor.revision,error));
    sqlite3 *other=nullptr;CHECK(sqlite3_open(path,&other)==SQLITE_OK);
    sqlite3_busy_timeout(db,5000);sqlite3_busy_timeout(other,5000);
    Store second(other,verify,hash);Result a,b;
    std::atomic<bool> start(false);
    std::thread first([&]{while(!start.load()){}a=s.authorize(1,"abc",1,1,0,"2468",100000,true);});
    std::thread next([&]{while(!start.load()){}b=second.authorize(1,"def",1,1,0,"2468",100000,true);});
    start=true;first.join();next.join();
    CHECK(a.ok&&b.ok);CHECK((a.response==6)+(b.response==6)==1);
    CHECK(get(s,1,visitor.id).remaining==0);
    const std::string accepted=a.response==6?"abc":"def";
    const std::string eventId=a.response==6?a.eventId:b.eventId;
    CHECK(sqlite3_close(other)==SQLITE_OK);CHECK(sqlite3_close(db)==SQLITE_OK);
    CHECK(sqlite3_open(path,&db)==SQLITE_OK);Store reopened(db,verify,hash);
    auto duplicate=reopened.authorize(1,accepted,1,1,0,"2468",100001,true);
    CHECK(duplicate.ok&&duplicate.duplicate&&duplicate.eventId==eventId);
    CHECK(get(reopened,1,visitor.id).remaining==0);
    LockoutPolicy policy;std::vector<LockoutState> states;
    CHECK(reopened.lockout(1,policy,states));
    CHECK(!policy.enabled&&policy.threshold==6&&policy.windowSeconds==60);
    CHECK(policy.durations[0]==60&&policy.durations[1]==600&&policy.durations[2]==1800&&policy.resetSeconds==3600);
    // Existing saved choices must survive reopen rather than adopting new defaults.
    policy.enabled=true;policy.threshold=1;policy.windowSeconds=90;
    policy.durations[0]=60;policy.durations[1]=1200;policy.durations[2]=3600;policy.resetSeconds=86400;
    CHECK(reopened.configureLockout(1,policy,0,error));
    auto locked=reopened.authorize(1,"abc",1,2,0,"9999",200000,false);
    CHECK(locked.locked&&locked.lockoutLevel==1);
    for(int level=2;level<=4;++level) {
        locked=reopened.authorize(1,"abc",1,level+1,0,"9999",locked.lockedUntil,false);
        CHECK(locked.ok&&locked.locked&&locked.lockoutLevel==std::min(3,level));
    }
    const auto until=locked.lockedUntil;
    CHECK(sqlite3_close(db)==SQLITE_OK);CHECK(sqlite3_open(path,&db)==SQLITE_OK);
    Store again(db,verify,hash);
    LockoutPolicy saved;CHECK(again.lockout(1,saved,states));
    CHECK(saved.enabled&&saved.threshold==1&&saved.windowSeconds==90);
    CHECK(saved.durations[0]==60&&saved.durations[1]==1200&&saved.durations[2]==3600&&saved.resetSeconds==86400);
    CHECK(again.authorize(1,"abc",1,6,0,"1357",until-1,false).locked);
    CHECK(again.resetLockout(1));CHECK(again.authorize(1,"abc",1,7,0,"1357",until,false).response==0);
    CHECK(sqlite3_close(db)==SQLITE_OK);std::remove(path);
}
int main(){try{tests();persistenceAndConcurrency();std::cout<<assertions<<" global-user policy checks passed\n";return 0;}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
