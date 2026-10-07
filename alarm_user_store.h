#ifndef ALARM_USER_STORE_H
#define ALARM_USER_STORE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
struct sqlite3;

namespace AlarmUsers {
constexpr int MaxUsers = 256;
constexpr int64_t DuplicateWindowMs = 10000;
struct Keypad { std::string source; int endpoint = 0; };
struct User {
    std::string id, name, hash;
    std::string schedule; // canonical JSON; empty = no schedule/expiry
    bool enabled = true;
    int64_t userRevision = 0;
    bool grantEnabled = true, owner = false, arm = true, disarm = true, allKeypads = false;
    std::vector<Keypad> keypads;
    bool apiArmDisarm = false; // REST alarm commands only; keypad eligibility is separate
    int64_t remaining = -1; // -1 unlimited; zero exhausted
    int64_t revision = 0;
};
struct LockoutPolicy {
    bool enabled = false;
    int threshold = 6, windowSeconds = 60;
    int durations[3] = {60, 600, 1800};
    int resetSeconds = 3600;
    int64_t revision = 0;
};
struct LockoutState {
    std::string source;
    int endpoint = 0, level = 0;
    int64_t until = 0, lastFailure = 0;
};
struct Result {
    bool locked = false;
    int lockoutLevel = 0;
    int64_t lockedUntil = 0;
    bool ok = false; // false = storage error, NEVER invalid code
    bool duplicate = false;
    int response = 5; // IAS not_ready
    User user;
    std::string eventId;
};
struct RestResult {
    bool ok = false; // storage/schedule errors are not credential rejections
    bool accepted = false;
    User user; // populated only on acceptance; never serialized wholesale
};
using Verify = std::function<bool(const std::string &, const std::string &)>;
using ScheduleCheck = std::function<int(const std::string &, int64_t)>; // -1 error, 0 denied, 1 allowed; now=0 validates
using Hash = std::function<std::string(const std::string &)>;

// Caller supplies the gateway DB connection; every mutation commits before success.
// alarm=0 lists/edits gateway identities; positive alarm IDs address grants.
class Store {
public:
    Store(sqlite3 *db, Verify verify, Hash hash, ScheduleCheck check = {});
    // Read-only opt-in check. Failure is not permission to fall back to legacy.
    bool managementEnabled(int alarm, bool &enabled);
    bool lockout(int alarm, LockoutPolicy &policy, std::vector<LockoutState> &states);
    bool configureLockout(int alarm, LockoutPolicy &policy, int64_t revision, std::string &error);
    bool resetLockout(int alarm);
    bool list(int alarm, std::vector<User> &users);
    // expectedRevision=0 creates; >0 edits. Empty pin preserves existing hash.
    // Every managed alarm must retain an enabled unrestricted owner grant.
    bool put(int alarm, User &user, const std::string &pin, int64_t expectedRevision,
             std::string &error);
    bool erase(int alarm, const std::string &uid, int64_t expectedRevision, int64_t userRevision);
    RestResult authorizeRest(int alarm, const std::string &pin, int64_t nowMs = -1, int mode = 0);
    bool restCode(int alarm, const std::string &pin, int64_t nowMs = -1); // API permission + eligibility; never consumes uses
    Result authorize(int alarm, const std::string &source, int endpoint, int sequence,
                     int mode, const std::string &pin, int64_t nowMs, bool alreadyDisarmed);
private:
    sqlite3 *db;
    Verify verify;
    Hash hash;
    ScheduleCheck scheduleCheck;
    bool init(int alarm);
    bool read(int alarm, std::vector<User> &users);
    bool activate(int alarm);
};
}
#endif


