#include "alarm_user_store.h"
#include <sqlite3.h>
#include <algorithm>
#include <utility>
#include <chrono>

namespace AlarmUsers {
namespace {
struct Statement {
    sqlite3_stmt *s = nullptr;
    Statement(sqlite3 *db, const char *sql) { sqlite3_prepare_v2(db, sql, -1, &s, nullptr); }
    ~Statement() { sqlite3_finalize(s); }
    bool valid() const { return s != nullptr; }
    void number(int n, int64_t v) { sqlite3_bind_int64(s, n, v); }
    void text(int n, const std::string &v) { sqlite3_bind_text(s, n, v.data(), int(v.size()), SQLITE_TRANSIENT); }
    int step() { return s ? sqlite3_step(s) : SQLITE_ERROR; }
    int64_t number(int n) { return sqlite3_column_int64(s, n); }
    std::string text(int n) { const auto *v = sqlite3_column_text(s, n); return v ? reinterpret_cast<const char *>(v) : ""; }
};
bool exec(sqlite3 *db, const char *sql) { return db && sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK; }
struct Transaction {
    sqlite3 *db;
    bool active;
    explicit Transaction(sqlite3 *d) : db(d), active(exec(d, "BEGIN IMMEDIATE")) {}
    ~Transaction() { if (active) exec(db, "ROLLBACK"); }
    bool commit() { if (!active || !exec(db, "COMMIT")) return false; active = false; return true; }
};
bool validPin(const std::string &pin) {
    return pin.size() >= 4 && pin.size() <= 16 &&
        std::all_of(pin.begin(), pin.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}
Store::Store(sqlite3 *d, Verify v, Hash h, ScheduleCheck check)
    : db(d), verify(std::move(v)), hash(std::move(h)), scheduleCheck(std::move(check)) {}

bool Store::managementEnabled(int alarm, bool &enabled) {
    enabled = false;
    if (!db || alarm < 1 || alarm > 255) return false;
    // An unconverted experimental database must never fall through to legacy PINs.
    Statement old(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='alarm_user_management_v1'");
    if (!old.valid()) return false;
    if (old.step() == SQLITE_ROW) {
        Statement rows(db, "SELECT 1 FROM alarm_user_management_v1 LIMIT 1");
        if (!rows.valid() || rows.step() != SQLITE_DONE) return false;
    }
    Statement exists(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='alarm_user_management_v2'");
    if (!exists.valid()) return false;
    const int rc = exists.step();
    if (rc == SQLITE_DONE) return true;
    if (rc != SQLITE_ROW) return false;
    Statement s(db, "SELECT 1 FROM alarm_user_management_v2 WHERE alarm=?");
    if (!s.valid()) return false;
    s.number(1, alarm); const int found = s.step();
    enabled = found == SQLITE_ROW;
    return enabled || found == SQLITE_DONE;
}

bool Store::activate(int alarm) {
    Statement s(db, "INSERT OR IGNORE INTO alarm_user_management_v2 VALUES(?)");
    if (!s.valid()) return false;
    s.number(1, alarm); return s.step() == SQLITE_DONE;
}

bool Store::init(int alarm) {
    bool enabled = false;
    if (!db || alarm < 0 || alarm > 255 || !managementEnabled(alarm ? alarm : 1, enabled)) return false;
    return exec(db,
        "CREATE TABLE IF NOT EXISTS gateway_users_v2 (uid TEXT PRIMARY KEY,name TEXT NOT NULL,hash TEXT NOT NULL,"
        "enabled INTEGER NOT NULL CHECK(enabled IN (0,1)),revision INTEGER NOT NULL CHECK(revision>0));"
        "CREATE TABLE IF NOT EXISTS alarm_user_management_v2 (alarm INTEGER PRIMARY KEY CHECK(alarm BETWEEN 1 AND 255));"
        "CREATE TABLE IF NOT EXISTS alarm_user_grants_v2 (alarm INTEGER NOT NULL CHECK(alarm BETWEEN 1 AND 255),uid TEXT NOT NULL,"
        "enabled INTEGER NOT NULL CHECK(enabled IN (0,1)),owner INTEGER NOT NULL CHECK(owner IN (0,1)),"
        "arm INTEGER NOT NULL CHECK(arm IN (0,1)),disarm INTEGER NOT NULL CHECK(disarm IN (0,1)),"
        "api_arm_disarm INTEGER NOT NULL CHECK(api_arm_disarm IN (0,1)),all_keypads INTEGER NOT NULL CHECK(all_keypads IN (0,1)),"
        "remaining INTEGER NOT NULL CHECK(remaining>=-1),schedule TEXT NOT NULL,revision INTEGER NOT NULL CHECK(revision>0),"
        "PRIMARY KEY(alarm,uid),FOREIGN KEY(uid) REFERENCES gateway_users_v2(uid));"
        "CREATE TABLE IF NOT EXISTS alarm_user_keypads_v2 (alarm INTEGER NOT NULL,uid TEXT NOT NULL,source TEXT NOT NULL,"
        "endpoint INTEGER NOT NULL CHECK(endpoint BETWEEN 1 AND 240),PRIMARY KEY(alarm,uid,source,endpoint));"
        "CREATE TABLE IF NOT EXISTS alarm_user_requests_v1 (alarm INTEGER NOT NULL,source TEXT NOT NULL,endpoint INTEGER NOT NULL,"
        "sequence INTEGER NOT NULL,mode INTEGER NOT NULL,created INTEGER NOT NULL,uid TEXT NOT NULL,response INTEGER NOT NULL,"
        "eventid TEXT NOT NULL,PRIMARY KEY(alarm,source,endpoint,sequence));"
        "CREATE TABLE IF NOT EXISTS alarm_lockout_policy_v1 (alarm INTEGER PRIMARY KEY,enabled INTEGER NOT NULL,threshold INTEGER NOT NULL,"
        "window INTEGER NOT NULL,d1 INTEGER NOT NULL,d2 INTEGER NOT NULL,d3 INTEGER NOT NULL,reset INTEGER NOT NULL,revision INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS alarm_lockout_state_v1 (alarm INTEGER NOT NULL,source TEXT NOT NULL,endpoint INTEGER NOT NULL,"
        "level INTEGER NOT NULL,until INTEGER NOT NULL,last_failure INTEGER NOT NULL,PRIMARY KEY(alarm,source,endpoint));"
        "CREATE TABLE IF NOT EXISTS alarm_lockout_failures_v1 (alarm INTEGER NOT NULL,source TEXT NOT NULL,endpoint INTEGER NOT NULL,created INTEGER NOT NULL);");
}

bool Store::read(int alarm, std::vector<User> &users) {
    users.clear();
    Statement s(db, alarm ?
        "SELECT u.uid,u.name,u.hash,u.enabled,u.revision,g.enabled,g.owner,g.arm,g.disarm,g.api_arm_disarm,g.all_keypads,"
        "g.remaining,g.schedule,g.revision FROM gateway_users_v2 u JOIN alarm_user_grants_v2 g ON g.uid=u.uid WHERE g.alarm=? ORDER BY u.uid" :
        "SELECT uid,name,hash,enabled,revision FROM gateway_users_v2 ORDER BY uid");
    if (!s.valid()) return false;
    if (alarm) s.number(1,alarm);
    int rc;
    while ((rc=s.step())==SQLITE_ROW) {
        User u;u.id=s.text(0);u.name=s.text(1);u.hash=s.text(2);u.enabled=s.number(3)==1;u.userRevision=s.number(4);
        if (alarm) {
            u.grantEnabled=s.number(5)==1;u.owner=s.number(6)==1;u.arm=s.number(7)==1;u.disarm=s.number(8)==1;
            u.apiArmDisarm=s.number(9)==1;u.allKeypads=s.number(10)==1;u.remaining=s.number(11);u.schedule=s.text(12);u.revision=s.number(13);
            Statement pads(db,"SELECT source,endpoint FROM alarm_user_keypads_v2 WHERE alarm=? AND uid=? ORDER BY source,endpoint");
            if (!pads.valid()) return false;
            pads.number(1,alarm);pads.text(2,u.id);int pr;
            while((pr=pads.step())==SQLITE_ROW) u.keypads.push_back({pads.text(0),int(pads.number(1))});
            if(pr!=SQLITE_DONE)return false;
        }
        users.push_back(u);
    }
    return rc==SQLITE_DONE;
}
bool Store::list(int alarm,std::vector<User> &users) {
    Transaction t(db);return t.active && init(alarm) && read(alarm,users) && t.commit();
}

namespace {
bool ownersValid(sqlite3 *db) {
    // Ownership is a role on a grant, independent of ID, ordering or display name.
    Statement s(db,"SELECT 1 FROM alarm_user_management_v2 m WHERE NOT EXISTS ("
        "SELECT 1 FROM alarm_user_grants_v2 g JOIN gateway_users_v2 u ON u.uid=g.uid WHERE g.alarm=m.alarm AND "
        "g.owner=1 AND g.enabled=1 AND u.enabled=1 AND g.arm=1 AND g.disarm=1 AND g.api_arm_disarm=1 AND g.remaining=-1 AND g.schedule='') LIMIT 1");
    return s.valid() && s.step()==SQLITE_DONE;
}
bool validId(const std::string &id) {
    return id.size()==32 && std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool validName(const std::string &name) {
    return !name.empty() && name.size()<=64 && std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;});
}
bool validKeypad(const Keypad &p) {
    return !p.source.empty() && p.source.size()<=16 && p.endpoint>=1 && p.endpoint<=240 &&
        std::all_of(p.source.begin(),p.source.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}) &&
        (p.source.size()==1 || p.source[0]!='0');
}
}

// One atomic transaction supports creating an identity with its first grant,
// attaching an existing identity, and editing a projected identity/grant pair.
// alarm=0 edits identity only; grant edits require BOTH revisions.
bool Store::put(int alarm,User &u,const std::string &pin,int64_t revision,std::string &error) {
    error="invalid_user";
    if(alarm<0||alarm>255||!validName(u.name)||revision<0||u.userRevision<0||
       (!u.id.empty()&&!validId(u.id))||(!pin.empty()&&!validPin(pin)))return false;
    if(alarm && (u.remaining < -1 || u.remaining>1000000 || u.keypads.size()>256 || (u.allKeypads&&!u.keypads.empty())))return false;
    for(const auto &p:u.keypads)if(!validKeypad(p))return false;
    if(alarm && !u.schedule.empty() && (u.schedule.size()>8192||!scheduleCheck||scheduleCheck(u.schedule,0)!=1)) {
        error="invalid_schedule";return false;
    }
    if(alarm && u.owner && (!u.enabled||!u.grantEnabled||!u.arm||!u.disarm||!u.apiArmDisarm||u.remaining!=-1||!u.schedule.empty())) {
        error="owner_must_be_unrestricted";return false;
    }
    error="storage_error";Transaction t(db);std::vector<User> identities,grants;
    if(!t.active||!init(alarm)||!read(0,identities)||(alarm&&!read(alarm,grants)))return false;
    auto old=std::find_if(identities.begin(),identities.end(),[&](const User &x){return x.id==u.id;});
    auto grant=std::find_if(grants.begin(),grants.end(),[&](const User &x){return x.id==u.id;});
    const bool creating=old==identities.end();
    if((creating && (!u.id.empty()||u.userRevision!=0)) || (!creating && old->userRevision!=u.userRevision) ||
       (alarm && (grant==grants.end()?revision!=0:grant->revision!=revision)) || (!alarm&&revision!=u.userRevision)) {
        error="revision_conflict";return false;
    }
    if(creating && identities.size()>=MaxUsers){error="user_limit";return false;}
    if(creating && pin.empty()){error="pin_required";return false;}
    // Attaching requires the current credential, not hash equality (salted hashes).
    const bool attaching=alarm && !creating && grant==grants.end();
    if(attaching && (pin.empty()||!verify(old->hash,pin))){error="current_pin_required";return false;}
    if(!pin.empty()) {
        // Union of all existing grants plus the proposed new alarm. Disabled
        // identities and grants still reserve the PIN within those alarms.
        Statement peers(db,"SELECT DISTINCT u.hash FROM gateway_users_v2 u JOIN alarm_user_grants_v2 g ON g.uid=u.uid "
            "WHERE u.uid<>? AND (g.alarm=? OR g.alarm IN (SELECT alarm FROM alarm_user_grants_v2 WHERE uid=?))");
        if(!peers.valid())return false;
        peers.text(1,u.id);peers.number(2,alarm);peers.text(3,u.id);int rc;
        while((rc=peers.step())==SQLITE_ROW)if(verify(peers.text(0),pin)){error="pin_already_assigned";return false;}
        if(rc!=SQLITE_DONE)return false;
    }
    const bool identityChanged=creating||u.name!=old->name||u.enabled!=old->enabled||(!pin.empty()&&!attaching);
    u.hash=creating||(!pin.empty()&&!attaching)?hash(pin):old->hash;
    if(u.hash.empty())return false;
    if(creating) {
        Statement id(db,"SELECT lower(hex(randomblob(16)))");if(id.step()!=SQLITE_ROW)return false;u.id=id.text(0);
    }
    if(identityChanged) {
        Statement save(db,"INSERT OR REPLACE INTO gateway_users_v2 VALUES(?,?,?,?,?)");
        if(!save.valid())return false;
        save.text(1,u.id);save.text(2,u.name);save.text(3,u.hash);save.number(4,u.enabled);save.number(5,u.userRevision+1);
        if(save.step()!=SQLITE_DONE)return false;
        ++u.userRevision;
    }
    if(alarm) {
        Statement save(db,"INSERT OR REPLACE INTO alarm_user_grants_v2 VALUES(?,?,?,?,?,?,?,?,?,?,?)");
        if(!save.valid())return false;
        save.number(1,alarm);save.text(2,u.id);save.number(3,u.grantEnabled);save.number(4,u.owner);save.number(5,u.arm);save.number(6,u.disarm);
        save.number(7,u.apiArmDisarm);save.number(8,u.allKeypads);save.number(9,u.remaining);save.text(10,u.schedule);save.number(11,revision+1);
        if(save.step()!=SQLITE_DONE)return false;
        Statement clear(db,"DELETE FROM alarm_user_keypads_v2 WHERE alarm=? AND uid=?");
        if(!clear.valid())return false;
        clear.number(1,alarm);clear.text(2,u.id);if(clear.step()!=SQLITE_DONE)return false;
        for(const auto &p:u.keypads) {
            Statement add(db,"INSERT INTO alarm_user_keypads_v2 VALUES(?,?,?,?)");if(!add.valid())return false;
            add.number(1,alarm);add.text(2,u.id);add.text(3,p.source);add.number(4,p.endpoint);if(add.step()!=SQLITE_DONE)return false;
        }
        if(!activate(alarm))return false;
        u.revision=revision+1;
    }
    if(!ownersValid(db)){error="last_unrestricted_owner_required";return false;}
    if(!t.commit())return false;
    error.clear();return true;
}

bool Store::erase(int alarm,const std::string &uid,int64_t revision,int64_t userRevision) {
    if(!validId(uid)||revision<1||userRevision<1)return false;
    Transaction t(db);if(!t.active||!init(alarm))return false;
    Statement s(db,alarm?"DELETE FROM alarm_user_grants_v2 WHERE alarm=? AND uid=? AND revision=? AND EXISTS(SELECT 1 FROM gateway_users_v2 WHERE uid=? AND revision=?)":
        "DELETE FROM gateway_users_v2 WHERE ?=0 AND uid=? AND revision=? AND uid=? AND revision=? AND NOT EXISTS(SELECT 1 FROM alarm_user_grants_v2 WHERE uid=gateway_users_v2.uid)");
    if(!s.valid())return false;
    s.number(1,alarm);s.text(2,uid);s.number(3,revision);s.text(4,uid);s.number(5,userRevision);
    if(s.step()!=SQLITE_DONE||sqlite3_changes(db)!=1||!ownersValid(db))return false;
    Statement clear(db,"DELETE FROM alarm_user_keypads_v2 WHERE alarm=? AND uid=?");
    if(!clear.valid())return false;
    clear.number(1,alarm);clear.text(2,uid);return clear.step()==SQLITE_DONE&&t.commit();
}
RestResult Store::authorizeRest(int alarm,const std::string &pin,int64_t now,int mode) {
    RestResult result;bool managed=false;
    if(mode<0||mode>3||!managementEnabled(alarm,managed)||!managed)return result;
    if(now==-1)now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<User> users;if(!list(alarm,users))return result;
    for(const auto &u:users) {
        if(!u.apiArmDisarm||!u.enabled||!u.grantEnabled||u.remaining==0||!(mode==0?u.disarm:u.arm)||!verify(u.hash,pin))continue;
        const int allowed=u.schedule.empty()?1:(now>0&&scheduleCheck?scheduleCheck(u.schedule,now):-1);
        if(allowed<0)return result;
        if(allowed==1){result.accepted=true;result.user=u;}break;
    }
    result.ok=true;return result;
}
bool Store::restCode(int alarm,const std::string &pin,int64_t now) {
    const auto r=authorizeRest(alarm,pin,now);return r.ok&&r.accepted;
}
namespace {
bool validPolicy(const LockoutPolicy &p) {
    return p.threshold >= 1 && p.threshold <= 100 && p.windowSeconds >= 1 && p.windowSeconds <= 3600 &&
        p.durations[0] >= 1 && p.durations[0] <= p.durations[1] && p.durations[1] <= p.durations[2] &&
        p.durations[2] <= 3600 && p.resetSeconds >= 3600 && p.resetSeconds <= 604800 && p.revision >= 0;
}
bool readPolicy(sqlite3 *db, int alarm, LockoutPolicy &p) {
    p = LockoutPolicy{};
    Statement q(db,"SELECT enabled,threshold,window,d1,d2,d3,reset,revision FROM alarm_lockout_policy_v1 WHERE alarm=?");
    if (!q.valid()) return false;
    q.number(1,alarm); const int rc=q.step();
    if (rc==SQLITE_DONE) return true;
    if (rc!=SQLITE_ROW || (q.number(0)!=0 && q.number(0)!=1)) return false;
    p.enabled=q.number(0)==1; p.threshold=int(q.number(1)); p.windowSeconds=int(q.number(2));
    for(int i=0;i<3;++i) p.durations[i]=int(q.number(3+i));
    p.resetSeconds=int(q.number(6)); p.revision=q.number(7);
    return validPolicy(p);
}
bool clearFailures(sqlite3 *db,int alarm,const std::string &source,int endpoint) {
    Statement q(db,"DELETE FROM alarm_lockout_failures_v1 WHERE alarm=? AND source=? AND endpoint=?");
    if (!q.valid()) return false;
    q.number(1,alarm);q.text(2,source);q.number(3,endpoint);return q.step()==SQLITE_DONE;
}
}
bool Store::lockout(int alarm, LockoutPolicy &p, std::vector<LockoutState> &states) {
    Transaction t(db); states.clear();
    if (!t.active || !init(alarm) || !readPolicy(db,alarm,p)) return false;
    Statement q(db,"SELECT source,endpoint,level,until,last_failure FROM alarm_lockout_state_v1 WHERE alarm=?");
    if (!q.valid()) return false;
    q.number(1,alarm);int rc;
    while((rc=q.step())==SQLITE_ROW) {
        LockoutState st;st.source=q.text(0);st.endpoint=int(q.number(1));st.level=int(q.number(2));
        st.until=q.number(3);st.lastFailure=q.number(4);states.push_back(st);
    }
    return rc==SQLITE_DONE && t.commit();
}
bool Store::configureLockout(int alarm, LockoutPolicy &p, int64_t revision, std::string &error) {
    error="invalid_lockout_policy";
    if (!validPolicy(p) || revision<0) return false;
    error="storage_error";Transaction t(db);LockoutPolicy old;bool managed=false;
    if (!t.active || !init(alarm) || !managementEnabled(alarm,managed) || !readPolicy(db,alarm,old)) return false;
    if (!managed && p.enabled) {error="managed_users_required";return false;}
    if (old.revision!=revision) {error="revision_conflict";return false;}
    Statement q(db,"INSERT OR REPLACE INTO alarm_lockout_policy_v1 VALUES(?,?,?,?,?,?,?,?,?)");
    if (!q.valid()) return false;
    q.number(1,alarm);q.number(2,p.enabled);q.number(3,p.threshold);q.number(4,p.windowSeconds);
    for(int i=0;i<3;++i)q.number(5+i,p.durations[i]);
    q.number(8,p.resetSeconds);q.number(9,revision+1);
    if(q.step()!=SQLITE_DONE) return false;
    // Disabling is explicit recovery. Editing enabled policy preserves active deadlines.
    if (!p.enabled) {
        Statement a(db,"DELETE FROM alarm_lockout_state_v1 WHERE alarm=?");
        Statement b(db,"DELETE FROM alarm_lockout_failures_v1 WHERE alarm=?");
        if(!a.valid()||!b.valid())return false;
        a.number(1,alarm);b.number(1,alarm);
        if(a.step()!=SQLITE_DONE||b.step()!=SQLITE_DONE)return false;
    }
    if(!t.commit())return false;
    p.revision=revision+1;error.clear();return true;
}
bool Store::resetLockout(int alarm) {
    Transaction t(db);if(!t.active||!init(alarm))return false;
    Statement a(db,"DELETE FROM alarm_lockout_state_v1 WHERE alarm=?");
    Statement b(db,"DELETE FROM alarm_lockout_failures_v1 WHERE alarm=?");
    if(!a.valid()||!b.valid())return false;
    a.number(1,alarm);b.number(1,alarm);
    return a.step()==SQLITE_DONE && b.step()==SQLITE_DONE && t.commit();
}
Result Store::authorize(int alarm,const std::string &source,int endpoint,int sequence,
                        int mode,const std::string &pin,int64_t now,bool alreadyDisarmed) {
    Result result;
    bool managed = false;
    if (!managementEnabled(alarm, managed) || !managed) return result;
    if (source.empty() || source.size()>32 || endpoint<1 || endpoint>240 || sequence<0 || sequence>255 ||
        mode<0 || mode>3 || now<=DuplicateWindowMs) return result;
    Transaction t(db); std::vector<User> users;
    if (!t.active || !init(alarm) || !read(alarm,users)) return result;
    LockoutPolicy policy;
    if (!readPolicy(db,alarm,policy)) return result;
    LockoutState state;state.source=source;state.endpoint=endpoint;
    if (policy.enabled) {
        Statement q(db,"SELECT level,until,last_failure FROM alarm_lockout_state_v1 WHERE alarm=? AND source=? AND endpoint=?");
        if(!q.valid())return Result{};
        q.number(1,alarm);q.text(2,source);q.number(3,endpoint);const int found=q.step();
        if(found==SQLITE_ROW) {
            state.level=int(q.number(0));state.until=q.number(1);state.lastFailure=q.number(2);
            if(state.level<0||state.level>3||state.until<0||state.lastFailure>now)return Result{};
        } else if(found!=SQLITE_DONE)return Result{};
        result.locked=state.until>now;
    }
    // Match disabled/exhausted users too, so retries of the final use can be recognized.
    User matched;
    // During lockout do not evaluate the submitted credential at all.
    if (!result.locked) for (const auto &u:users) if (verify(u.hash,pin)) { matched=u; break; }
    Statement previous(db,"SELECT mode,created,uid,response,eventid FROM alarm_user_requests_v1 "
        "WHERE alarm=? AND source=? AND endpoint=? AND sequence=?");
    if (!previous.valid()) return result;
    previous.number(1,alarm); previous.text(2,source); previous.number(3,endpoint); previous.number(4,sequence);
    int rc=previous.step();
    if (rc==SQLITE_ROW) {
        const auto age=now-previous.number(1);
        if (age<0) return result; // clock moved backward: never re-admit uncertain receipt
        if (age<=DuplicateWindowMs) {
            if (previous.number(0)!=mode || (!result.locked && previous.text(2)!=matched.id)) return result;
            result.response=result.locked ? 4 : int(previous.number(3)); result.eventId=previous.text(4);
            result.user=matched; result.duplicate=true; result.ok=t.commit(); return result;
        }
    } else if (rc!=SQLITE_DONE) return result;
    previous.step(); // finish read before writes/commit
    result.response=4;
    if (policy.enabled) {
        if(state.until>now) {
            result.locked=true; // No timer extension, credential check or usage mutation.
        } else {
            if(now-state.lastFailure>=int64_t(policy.resetSeconds)*1000)state.level=0;
            if(matched.id.empty()) {
                Statement prune(db,"DELETE FROM alarm_lockout_failures_v1 WHERE alarm=? AND source=? AND endpoint=? AND created<=?");
                Statement add(db,"INSERT INTO alarm_lockout_failures_v1 VALUES(?,?,?,?)");
                Statement count(db,"SELECT count(*) FROM alarm_lockout_failures_v1 WHERE alarm=? AND source=? AND endpoint=?");
                if(!prune.valid()||!add.valid()||!count.valid())return Result{};
                for(auto q2:{&prune,&add,&count}) {q2->number(1,alarm);q2->text(2,source);q2->number(3,endpoint);}
                prune.number(4,now-int64_t(policy.windowSeconds)*1000);add.number(4,now);
                if(prune.step()!=SQLITE_DONE||add.step()!=SQLITE_DONE||count.step()!=SQLITE_ROW)return Result{};
                const int failures=int(count.number(0));count.step();
                state.lastFailure=now;
                if(failures>=policy.threshold) {
                    state.level=std::min(3,state.level+1);
                    state.until=now+int64_t(policy.durations[state.level-1])*1000;
                    result.locked=true;
                    if(!clearFailures(db,alarm,source,endpoint))return Result{};
                }
            } else if(!clearFailures(db,alarm,source,endpoint))return Result{};
            Statement saveState(db,"INSERT OR REPLACE INTO alarm_lockout_state_v1 VALUES(?,?,?,?,?,?)");
            if(!saveState.valid())return Result{};
            saveState.number(1,alarm);saveState.text(2,source);saveState.number(3,endpoint);
            saveState.number(4,state.level);saveState.number(5,state.until);saveState.number(6,state.lastFailure);
            if(saveState.step()!=SQLITE_DONE)return Result{};
        }
        if(result.locked){result.lockedUntil=state.until;result.lockoutLevel=state.level;}
    }
    int scheduled = 1;
    if (!result.locked && !matched.id.empty() && !matched.schedule.empty()) {
        scheduled = scheduleCheck ? scheduleCheck(matched.schedule,now) : -1;
        if (scheduled < 0) return result; // policy/clock failure is not_ready, never a close request
    }
    if (!result.locked && !matched.id.empty() && matched.enabled && matched.grantEnabled && (mode==0?matched.disarm:matched.arm) &&
        (matched.allKeypads || std::any_of(matched.keypads.begin(),matched.keypads.end(),[&](const Keypad &p){return p.source==source && p.endpoint==endpoint;})) && matched.remaining!=0 && scheduled == 1) {
        result.response=(mode==0 && alreadyDisarmed)?6:mode;
        // Only accepted physical keypad DISARM consumes a use. REST and ARM do not.
        if (mode==0 && matched.remaining>0) --matched.remaining;
        Statement consume(db,"UPDATE alarm_user_grants_v2 SET remaining=?,revision=revision+1 WHERE alarm=? AND uid=? AND revision=?");
        if (!consume.valid()) return Result{};
        consume.number(1,matched.remaining); consume.number(2,alarm); consume.text(3,matched.id); consume.number(4,matched.revision);
        if (consume.step()!=SQLITE_DONE || sqlite3_changes(db)!=1) return Result{};
        ++matched.revision; result.user=matched;
    }
    Statement clean(db,"DELETE FROM alarm_user_requests_v1 WHERE created<?");
    if (!clean.valid()) return Result{};
    clean.number(1,now-DuplicateWindowMs);
    if (clean.step()!=SQLITE_DONE) return Result{};
    Statement save(db,"INSERT OR REPLACE INTO alarm_user_requests_v1 VALUES(?,?,?,?,?,?,?,?,lower(hex(randomblob(16))))");
    if (!save.valid()) return Result{};
    save.number(1,alarm); save.text(2,source); save.number(3,endpoint); save.number(4,sequence);
    save.number(5,mode); save.number(6,now); save.text(7,matched.id); save.number(8,result.response);
    if (save.step()!=SQLITE_DONE) return Result{};
    Statement id(db,"SELECT eventid FROM alarm_user_requests_v1 WHERE alarm=? AND source=? AND endpoint=? AND sequence=?");
    if (!id.valid()) return Result{};
    id.number(1,alarm); id.text(2,source); id.number(3,endpoint); id.number(4,sequence);
    if (id.step()!=SQLITE_ROW) return Result{};
    result.eventId=id.text(0); id.step();
    result.ok=t.commit(); return result;
}
}

