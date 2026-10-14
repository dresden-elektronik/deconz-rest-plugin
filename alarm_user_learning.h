#ifndef ALARM_USER_LEARNING_H
#define ALARM_USER_LEARNING_H

#include "alarm_user_store.h"
#include <algorithm>

namespace AlarmUsers {
constexpr int MaxLearningSessions = 16;
constexpr int MaxLearningSeconds = 120;
constexpr int64_t LearningStatusRetentionMs = 120000;

struct LearningSession {
    std::string id, uid, source, name;
    std::string state = "waiting", error, credentialId;
    int alarm = 0, endpoint = 0;
    int64_t userRevision = 0, deadline = 0, finished = 0;
    int lastSequence = -1, lastMode = -1;
    int64_t lastRequest = 0;
};

// Deadlines use monotonic time. Active sessions are deliberately not restored
// after restart; durable receipts in Store protect enrollment radio retries.
class LearningSessions {
public:
    bool begin(LearningSession session, int seconds, int64_t now, std::string &error) {
        refresh(now); error = "invalid_learning_session";
        if (seconds < 1 || seconds > MaxLearningSeconds || now < 0 || session.id.empty() || session.uid.empty() ||
            session.alarm < 1 || session.alarm > 255 || session.source.empty() || session.endpoint < 1 || session.endpoint > 240 ||
            session.name.empty() || session.userRevision < 1) return false;
        for (const auto &s : sessions) {
            if (s.state == "waiting" && (s.uid == session.uid || (s.source == session.source && s.endpoint == session.endpoint))) {
                error = "learning_busy"; return false;
            }
        }
        sessions.erase(std::remove_if(sessions.begin(), sessions.end(), [&](const LearningSession &s) {
            return s.uid == session.uid && s.alarm == session.alarm;
        }), sessions.end());
        if (sessions.size() >= MaxLearningSessions) {
            const auto old = std::find_if(sessions.begin(), sessions.end(), [](const LearningSession &s) { return s.state != "waiting"; });
            if (old == sessions.end()) { error = "learning_limit"; return false; }
            sessions.erase(old);
        }
        session.state = "waiting"; session.error.clear(); session.credentialId.clear();
        session.deadline = now + int64_t(seconds) * 1000; session.finished = 0;
        session.lastSequence = -1; session.lastMode = -1; session.lastRequest = 0;
        sessions.push_back(std::move(session)); error.clear(); return true;
    }

    bool status(int alarm, const std::string &uid, int64_t now, LearningSession &out) {
        refresh(now);
        const auto found = std::find_if(sessions.begin(), sessions.end(), [&](const LearningSession &s) { return s.alarm == alarm && s.uid == uid; });
        if (found == sessions.end()) return false;
        out = *found; return true;
    }

    bool cancel(int alarm, const std::string &uid, const std::string &sessionId, int64_t now, std::string &error) {
        refresh(now); error = "learning_session_conflict";
        for (auto &s : sessions) if (s.alarm == alarm && s.uid == uid && s.id == sessionId) {
            if (s.state != "waiting") { error = "learning_not_waiting"; return false; }
            s.state = "cancelled"; s.finished = now; error.clear(); return true;
        }
        return false;
    }

    template <class Capture>
    bool intercept(int alarm, const std::string &source, int endpoint, int sequence, int mode,
                   const std::string &value, int64_t now, Capture capture) {
        refresh(now);
        for (auto &s : sessions) {
            if (s.alarm != alarm || s.source != source || s.endpoint != endpoint) continue;
            if (s.state != "waiting") {
                // Also suppress a retry of a storage-failed capture in this process.
                if (s.lastSequence == sequence && s.lastMode == mode && now >= s.lastRequest &&
                    now - s.lastRequest <= DuplicateWindowMs) return true;
                continue;
            }
            const auto result = capture(s, value);
            s.lastSequence = sequence; s.lastMode = mode; s.lastRequest = now;
            if (result.duplicate) return true;
            if (result.ok && result.error == "rfid_required") return true; // Ignore a PIN; keep waiting for a tag.
            s.finished = now;
            if (result.ok && result.error.empty() && !result.credential.id.empty()) {
                s.state = "learned"; s.credentialId = result.credential.id; s.userRevision = result.userRevision;
            } else {
                s.state = "failed"; s.error = result.error.empty() ? "storage_error" : result.error;
            }
            return true;
        }
        return false;
    }

private:
    std::vector<LearningSession> sessions;
    void refresh(int64_t now) {
        for (auto &s : sessions) if (s.state == "waiting" && now >= s.deadline) {
            s.state = "expired"; s.finished = s.deadline;
        }
        sessions.erase(std::remove_if(sessions.begin(), sessions.end(), [&](const LearningSession &s) {
            return s.state != "waiting" && now >= s.finished && now - s.finished > LearningStatusRetentionMs;
        }), sessions.end());
    }
};
}
#endif
