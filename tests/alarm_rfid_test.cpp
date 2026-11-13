#include "alarm_user_store.h"
#include "alarm_user_learning.h"
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace AlarmUsers;
static std::atomic<int> assertions{0};
#define CHECK(x) do { ++assertions; if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while (0)

static std::string derive(const std::string &value, const std::string &salt) {
    unsigned char bytes[32];
    if (!EVP_PBE_scrypt(value.data(), value.size(), reinterpret_cast<const unsigned char *>(salt.data()), salt.size(), 1024, 8, 1, 0, bytes, sizeof bytes)) return "";
    std::ostringstream out; for (auto b : bytes) out << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return out.str();
}
static std::string hash(const std::string &value) {
    unsigned char bytes[8]; CHECK(RAND_bytes(bytes, sizeof bytes) == 1);
    std::ostringstream salt; for (auto b : bytes) salt << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return salt.str() + ":" + derive(value, salt.str());
}
static bool verify(const std::string &encoded, const std::string &value) {
    const auto at = encoded.find(':');
    return at != std::string::npos && encoded.substr(at + 1) == derive(value, encoded.substr(0, at));
}
static void sql(sqlite3 *db, const std::string &command) { CHECK(sqlite3_exec(db, command.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK); }
static User get(Store &store, int alarm, const std::string &uid) {
    std::vector<User> users; CHECK(store.list(alarm, users));
    for (const auto &u : users) if (u.id == uid) return u;
    throw std::runtime_error("missing user");
}
static User add(Store &store, int alarm, const std::string &pin, bool owner = false) {
    User u; u.name = owner ? "Owner" : "Visitor"; u.owner = owner; u.allKeypads = owner; u.apiArmDisarm = owner;
    std::string error; CHECK(store.put(alarm, u, pin, 0, error)); return u;
}
static LearningSession session(const User &u, const std::string &id = "session1", const std::string &source = "abc") {
    LearningSession s; s.id = id; s.uid = u.id; s.alarm = 1; s.source = source; s.endpoint = 1; s.name = "Daily tag"; s.userRevision = u.userRevision;
    return s;
}

static void credentialsAndPolicies() {
    std::string normalized;
    CHECK(normalizeRfid("+00AbCD", normalized) && normalized == "+00abcd");
    CHECK(normalizeRfid("+" + std::string(32, 'A'), normalized));
    for (const auto &bad : {"", "+", "1234", "+g123", "+ab cd", "+１２３４"}) CHECK(!normalizeRfid(bad, normalized) && normalized.empty());
    CHECK(!normalizeRfid("+" + std::string(33, 'a'), normalized));
    CHECK(!normalizeRfid(std::string("+ab\0cd", 6), normalized));
    sqlite3 *db = nullptr; CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK); sql(db, "PRAGMA foreign_keys=ON");
    Store store(db, verify, hash, [](const std::string &p, int64_t now) { return p == "window" ? (now == 0 || (now >= 100000 && now < 200000) ? 1 : 0) : -1; });
    const auto owner = add(store, 1, "1357", true); add(store, 2, "2468", true);
    // Simulate an existing v2 database before the additive RFID tables existed.
    sql(db, "DROP TABLE gateway_user_rfid_v1; DROP TABLE alarm_rfid_learning_receipts_v1");
    CHECK(store.authorizeRest(1, "1357", 100000).accepted);
    CHECK(get(store, 0, owner.id).userRevision == owner.userRevision);
    auto visitor = add(store, 1, "4567"); visitor.keypads = {{"abc", 1}, {"def", 2}}; visitor.remaining = 3; visitor.schedule = "window";
    std::string error; CHECK(store.put(1, visitor, "", visitor.revision, error));
    RfidCredential tag; int64_t revision = 0;
    CHECK(store.addRfid(visitor.id, "Daily tag", "+00AbCD", visitor.userRevision, tag, revision, error));
    CHECK(revision == visitor.userRevision + 1 && tag.id.size() == 32 && tag.hash.find("+00abcd") == std::string::npos);
    CHECK(get(store, 1, visitor.id).rfid.size() == 1);
    CHECK(!store.addRfid(visitor.id, "Duplicate", "+00abcd", revision, tag, revision, error) && error == "rfid_already_assigned");
    visitor = get(store, 1, visitor.id); tag = visitor.rfid.front();
    // Same person and same allowance, whichever credential or permitted keypad is used.
    const auto first = store.authorize(1, "abc", 1, 1, 0, "+00ABCD", 100000, true);
    CHECK(first.ok && first.response == 6 && first.user.id == visitor.id && first.user.remaining == 2);
    const auto retry = store.authorize(1, "abc", 1, 1, 0, "+00abcd", 100001, true);
    CHECK(retry.duplicate && retry.eventId == first.eventId && get(store, 1, visitor.id).remaining == 2);
    CHECK(store.authorize(1, "def", 2, 2, 0, "4567", 100002, true).response == 6);
    CHECK(store.authorize(1, "abc", 2, 3, 0, "+00abcd", 100003, true).response == 4);
    CHECK(store.authorize(1, "abc", 1, 4, 0, "+00abcd", 200000, true).response == 4);
    CHECK(store.authorize(2, "abc", 1, 5, 0, "+00abcd", 100005, true).response == 4);
    visitor = get(store, 0, visitor.id); visitor.allKeypads = true;
    CHECK(store.put(2, visitor, "4567", 0, error));
    CHECK(store.authorize(2, "fed", 1, 6, 0, "+00abcd", 100006, true).response == 6);
    CHECK(store.authorize(2, "fed", 1, 7, 0, "4567", 100007, true).response == 6);
    CHECK(get(store, 1, visitor.id).remaining == 1); // Independent alarm grants.
    visitor = get(store, 1, visitor.id); visitor.apiArmDisarm = true; visitor.arm = false;
    CHECK(store.put(1, visitor, "", visitor.revision, error));
    CHECK(store.authorizeRest(1, "4567", 100010).accepted && !store.authorizeRest(1, "+00abcd", 100010).accepted);
    CHECK(store.authorize(1, "abc", 1, 8, 1, "+00abcd", 100008, false).response == 4);
    CHECK(store.authorize(1, "abc", 1, 9, 0, "+abcd", 100009, false).response == 4); // Preserve leading zeros.
    visitor = get(store, 0, visitor.id); visitor.name = "Renamed";
    CHECK(store.put(0, visitor, "5678", visitor.userRevision, error)); // FK-safe identity/PIN update preserves tags.
    CHECK(get(store, 0, visitor.id).rfid.front().id == tag.id && store.authorizeRest(1, "5678", 100010).accepted);
    CHECK(!store.eraseRfid(visitor.id, tag.id, visitor.userRevision - 1, revision, error) && error == "revision_conflict");
    CHECK(store.eraseRfid(visitor.id, tag.id, visitor.userRevision, revision, error));
    CHECK(store.authorize(2, "fed", 1, 10, 0, "+00abcd", 100010, true).response == 4);
    CHECK(store.authorizeRest(1, "5678", 100010).accepted); // Lost-tag revocation leaves PIN intact.
    auto disabled = add(store, 0, "7777"); disabled.enabled = false;
    CHECK(store.put(0, disabled, "", disabled.userRevision, error));
    CHECK(store.addRfid(disabled.id, "Reserved", "+00abcd", disabled.userRevision, tag, revision, error));
    CHECK(!store.addRfid(owner.id, "Same tag", "+00ABCD", owner.userRevision, tag, revision, error) && error == "rfid_already_assigned");
    disabled = get(store, 0, disabled.id); CHECK(store.erase(0, disabled.id, disabled.userRevision, disabled.userRevision));
    CHECK(store.addRfid(owner.id, "Reassigned", "+00abcd", owner.userRevision, tag, revision, error));
    CHECK(!store.erase(0, owner.id, revision, revision)); // Failed user deletion rolls tag deletion back.
    CHECK(get(store, 0, owner.id).rfid.size() == 1);
    for (int i = 1; i < MaxRfidCredentials; ++i) {
        auto u = get(store, 0, owner.id);
        CHECK(store.addRfid(owner.id, "Extra", "+a" + std::to_string(i), u.userRevision, tag, revision, error));
    }
    auto u = get(store, 0, owner.id);
    CHECK(!store.addRfid(owner.id, "Too many", "+ffff", u.userRevision, tag, revision, error) && error == "rfid_limit");
    CHECK(sqlite3_close(db) == SQLITE_OK);
}

static void learningAndLockout() {
    sqlite3 *db = nullptr; CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    Store store(db, verify, hash); const auto owner = add(store, 1, "1357", true);
    auto visitor = add(store, 1, "4567"); visitor.keypads = {{"abc", 1}}; visitor.remaining = 2;
    std::string error; CHECK(store.put(1, visitor, "", visitor.revision, error));
    LearningSessions sessions; CHECK(sessions.begin(session(visitor), 60, 1000, error));
    CHECK(!sessions.begin(session(owner, "s2"), 60, 1000, error) && error == "learning_busy");
    auto other = session(visitor, "s3", "def"); other.alarm = 2;
    CHECK(!sessions.begin(other, 60, 1000, error) && error == "learning_busy");
    int sequence = 20; int64_t wall = 100000;
    auto capture = [&](const LearningSession &s, const std::string &value) {
        return store.captureRfid(s.alarm, s.uid, s.source, s.endpoint, sequence, 0, s.name, value, s.userRevision, wall);
    };
    CHECK(!sessions.intercept(1, "def", 1, sequence, 0, "+abcdef", 1001, capture));
    CHECK(!sessions.intercept(1, "abc", 2, sequence, 0, "+abcdef", 1001, capture));
    CHECK(sessions.intercept(1, "abc", 1, sequence, 0, "4567", 1002, capture));
    LearningSession status; CHECK(sessions.status(1, visitor.id, 1003, status) && status.state == "waiting");
    CHECK(get(store, 1, visitor.id).remaining == 2 && get(store, 0, visitor.id).rfid.empty());
    auto retry = store.authorize(1, "abc", 1, sequence, 0, "4567", wall + 1, true);
    CHECK(retry.ok && retry.duplicate && retry.response == 5 && retry.eventId.empty());
    ++sequence; ++wall;
    CHECK(sessions.intercept(1, "abc", 1, sequence, 0, "+ABCDEF", 1004, capture));
    CHECK(sessions.status(1, visitor.id, 1005, status) && status.state == "learned" && !status.credentialId.empty());
    CHECK(get(store, 1, visitor.id).remaining == 2);
    CHECK(sessions.intercept(1, "abc", 1, sequence, 0, "+ABCDEF", 1006, capture));
    CHECK(store.authorize(1, "abc", 1, sequence, 0, "+abcdef", wall + 1, true).response == 5);
    CHECK(store.authorize(1, "abc", 1, sequence + 1, 0, "+abcdef", wall + 2, true).response == 6);
    CHECK(get(store, 1, visitor.id).remaining == 1);
    auto u = get(store, 0, visitor.id);
    CHECK(sessions.begin(session(u, "expire"), 1, 20000, error));
    CHECK(sessions.status(1, u.id, 21000, status) && status.state == "expired");
    CHECK(!sessions.intercept(1, "abc", 1, 30, 0, "+1111", 21000, capture));
    CHECK(sessions.begin(session(u, "cancel"), 60, 22000, error));
    CHECK(!sessions.cancel(1, u.id, "expire", 22001, error));
    CHECK(sessions.cancel(1, u.id, "cancel", 22002, error));
    CHECK(!sessions.intercept(1, "abc", 1, 30, 0, "+1111", 22003, capture));
    CHECK(!sessions.begin(session(u), 0, 23000, error));
    CHECK(!sessions.begin(session(u), MaxLearningSeconds + 1, 23000, error));
    // Recheck the selected user's revision and keypad grant when the tag arrives.
    CHECK(sessions.begin(session(u, "stale"), 60, 24000, error));
    auto changed = u; changed.name = "Changed"; CHECK(store.put(0, changed, "", changed.userRevision, error));
    sequence = 40; wall = 200000;
    CHECK(sessions.intercept(1, "abc", 1, sequence, 0, "+1111", 24001, capture));
    CHECK(sessions.status(1, u.id, 24002, status) && status.state == "failed" && status.error == "revision_conflict");
    CHECK(store.authorize(1, "abc", 1, sequence, 0, "+abcdef", wall + 1, true).response == 5);
    visitor = get(store, 1, visitor.id); visitor.grantEnabled = false;
    CHECK(store.put(1, visitor, "", visitor.revision, error));
    const auto denied = store.captureRfid(1, visitor.id, "abc", 1, 41, 0, "Disabled", "+2222", visitor.userRevision, 200002);
    CHECK(denied.ok && denied.error == "keypad_not_allowed");
    visitor = get(store, 1, visitor.id); visitor.grantEnabled = true; CHECK(store.put(1, visitor, "", visitor.revision, error));
    LockoutPolicy policy; policy.enabled = true; policy.threshold = 1;
    CHECK(store.configureLockout(1, policy, 0, error));
    const auto locked = store.authorize(1, "abc", 1, 50, 0, "+ffff", 300000, false);
    CHECK(locked.ok && locked.locked);
    visitor = get(store, 0, visitor.id);
    const auto learned = store.captureRfid(1, visitor.id, "abc", 1, 51, 0, "New tag", "+3333", visitor.userRevision, 300001);
    CHECK(learned.ok && learned.error.empty());
    CHECK(store.authorize(1, "abc", 1, 52, 0, "+3333", 300002, false).lockedUntil == locked.lockedUntil);
    CHECK(get(store, 1, visitor.id).remaining == 1); // Enrollment did not consume a use or clear the lockout.
    CHECK(store.resetLockout(1));
    visitor = get(store, 1, visitor.id); visitor.remaining = 0; CHECK(store.put(1, visitor, "", visitor.revision, error));
    CHECK(store.authorize(1, "abc", 1, 53, 0, "+3333", 300003, false).response == 4);
    LockoutPolicy current; std::vector<LockoutState> states;
    CHECK(store.lockout(1, current, states) && !states.empty() && states.front().lastFailure == 0); // Known ineligible tag is not a wrong credential.
    sql(db, "CREATE TRIGGER deny_receipt BEFORE INSERT ON alarm_rfid_learning_receipts_v1 BEGIN SELECT RAISE(ABORT,'test storage fault'); END");
    visitor = get(store, 0, visitor.id); const auto oldRevision = visitor.userRevision;
    CHECK(sessions.begin(session(visitor, "fault"), 60, 40000, error)); sequence = 60; wall = 400000;
    CHECK(sessions.intercept(1, "abc", 1, sequence, 0, "+4444", 40001, capture));
    CHECK(sessions.status(1, visitor.id, 40002, status) && status.state == "failed");
    CHECK(get(store, 0, visitor.id).userRevision == oldRevision && get(store, 0, visitor.id).rfid.size() == 2);
    CHECK(sessions.intercept(1, "abc", 1, sequence, 0, "+4444", 40003, capture));
    sql(db, "DROP TRIGGER deny_receipt; PRAGMA query_only=ON");
    RfidCredential tag; int64_t revision;
    CHECK(!store.addRfid(visitor.id, "Blocked", "+5555", oldRevision, tag, revision, error) && error == "storage_error");
    sql(db, "PRAGMA query_only=OFF");
    CHECK(sqlite3_close(db) == SQLITE_OK);
}

static void sessionBounds() {
    LearningSessions sessions; std::string error;
    User u; u.userRevision = 1;
    for (int i = 0; i < MaxLearningSessions; ++i) {
        u.id = "user" + std::to_string(i);
        CHECK(sessions.begin(session(u, "s" + std::to_string(i), "a" + std::to_string(i)), 120, 1000, error));
    }
    u.id = "extra";
    CHECK(!sessions.begin(session(u, "extra", "b"), 120, 1001, error) && error == "learning_limit");
    CHECK(sessions.cancel(1, "user0", "s0", 1002, error));
    CHECK(sessions.begin(session(u, "extra", "b"), 120, 1003, error));
    LearningSession status;
    CHECK(!sessions.status(1, "user0", 1004, status)); // Terminal entry was evicted; active entries remain.
    CHECK(sessions.status(1, "user1", 121000, status) && status.state == "expired");
    CHECK(!sessions.status(1, "user1", 241001, status));
    // An invalid or duplicate capture cannot report enrollment success.
    CHECK(sessions.begin(session(u, "new", "b"), 60, 300000, error));
    CHECK(sessions.intercept(1, "b", 1, 1, 0, "+1", 300001, [](const LearningSession &, const std::string &) {
        RfidCapture result; result.ok = true; result.duplicate = true; return result;
    }));
    CHECK(sessions.status(1, u.id, 300002, status) && status.state == "waiting");
    CHECK(sessions.intercept(1, "b", 1, 2, 0, "+1", 300003, [](const LearningSession &, const std::string &) {
        RfidCapture result; result.ok = true; return result;
    }));
    CHECK(sessions.status(1, u.id, 300004, status) && status.state == "failed" && status.error == "storage_error");
}

static void persistenceAndConcurrency() {
    char path[] = "/tmp/alarm-rfid-XXXXXX"; const int fd = mkstemp(path); CHECK(fd >= 0); close(fd);
    sqlite3 *db = nullptr; CHECK(sqlite3_open(path, &db) == SQLITE_OK);
    Store store(db, verify, hash); add(store, 1, "1357", true);
    auto visitor = add(store, 1, "4567"); visitor.allKeypads = true; visitor.remaining = 1;
    std::string error; CHECK(store.put(1, visitor, "", visitor.revision, error));
    const auto learned = store.captureRfid(1, visitor.id, "abc", 1, 1, 0, "Pocket tag", "+00aabbcc", visitor.userRevision, 100000);
    CHECK(learned.ok && learned.error.empty()); CHECK(sqlite3_close(db) == SQLITE_OK);
    CHECK(sqlite3_open(path, &db) == SQLITE_OK); Store reopened(db, verify, hash);
    CHECK(get(reopened, 0, visitor.id).rfid.front().id == learned.credential.id);
    const auto retry = reopened.authorize(1, "abc", 1, 1, 0, "+00AABBCC", 100001, true);
    CHECK(retry.ok && retry.duplicate && retry.response == 5 && retry.eventId.empty());
    CHECK(get(reopened, 1, visitor.id).remaining == 1);
    CHECK(reopened.authorize(1, "abc", 1, 1, 0, "+00aabbcc", 99999, true).ok == false); // Backward clock fails closed.
    sqlite3 *other = nullptr; CHECK(sqlite3_open(path, &other) == SQLITE_OK);
    sqlite3_busy_timeout(db, 5000); sqlite3_busy_timeout(other, 5000);
    Store second(other, verify, hash); Result a, b; std::atomic<bool> start{false};
    std::thread one([&] { while (!start.load()) {} a = reopened.authorize(1, "abc", 1, 2, 0, "+00aabbcc", 120000, true); });
    std::thread two([&] { while (!start.load()) {} b = second.authorize(1, "def", 1, 2, 0, "4567", 120000, true); });
    start = true; one.join(); two.join();
    CHECK(a.ok && b.ok && (a.response == 6) + (b.response == 6) == 1);
    CHECK(get(reopened, 1, visitor.id).remaining == 0);
    // Two administrators cannot assign the same tag to two users concurrently.
    auto x = add(reopened, 0, "7777"); auto y = add(reopened, 0, "8888");
    bool addedX = false, addedY = false; std::string errorX, errorY; RfidCredential tagX, tagY; int64_t revX, revY;
    start = false;
    std::thread three([&] { while (!start.load()) {} addedX = reopened.addRfid(x.id, "A", "+deadbeef", x.userRevision, tagX, revX, errorX); });
    std::thread four([&] { while (!start.load()) {} addedY = second.addRfid(y.id, "B", "+DEADBEEF", y.userRevision, tagY, revY, errorY); });
    start = true; three.join(); four.join();
    CHECK(addedX != addedY && (addedX ? errorY : errorX) == "rfid_already_assigned");
    CHECK(sqlite3_close(other) == SQLITE_OK && sqlite3_close(db) == SQLITE_OK); std::remove(path);
}

int main() {
    try { credentialsAndPolicies(); learningAndLockout(); sessionBounds(); persistenceAndConcurrency();
        std::cout << assertions << " RFID credential and learning checks passed\n"; return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
