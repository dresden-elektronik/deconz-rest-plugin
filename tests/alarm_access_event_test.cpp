#include "alarm_user_event.h"
#include <QJsonDocument>
#include <iostream>
#include <stdexcept>

static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__)); } while (0)

int main()
{
    try {
        AlarmUsers::Result result;
        result.ok = true;
        result.response = 4;
        result.eventId = "synthetic-event";
        // Even a matched but ineligible user must never be exposed by a rejection.
        result.user.id = "synthetic-private-id";
        result.user.name = "synthetic-private-name";
        result.user.hash = "synthetic-private-hash";
        result.user.remaining = 9;
        auto event = [&](bool managed = true) {
            return AlarmUsers::accessEvent(managed, result, "1", "2", 0, 1700000000000);
        };
        CHECK(event(false).isEmpty());
        const auto denied = event();
        CHECK(denied.size() == 10);
        CHECK(denied["e"] == "access");
        CHECK(denied["result"] == "rejected");
        CHECK(denied["action"] == "invalid_code");
        CHECK(denied["event_id"] == "synthetic-event");
        CHECK(denied["uses_consumed"].toInt() == 0);
        CHECK(!denied.contains("user_id"));
        CHECK(!denied.contains("user_slot"));
        CHECK(!denied.contains("remaining_uses"));
        CHECK(!QJsonDocument::fromVariant(denied).toJson().contains("synthetic-private"));
        result.duplicate = true;
        CHECK(event().isEmpty());
        result.duplicate = false;
        result.ok = false;
        CHECK(event().isEmpty());
        result.ok = true;
        result.response = 5;
        CHECK(event().isEmpty());
        result.response = 99;
        CHECK(event().isEmpty());
        for (int response : {0, 1, 2, 3, 6}) {
            result.response = response;
            CHECK(!event().isEmpty());
            CHECK(event()["result"] == "accepted");
            CHECK(event()["user_id"] == "synthetic-private-id");
            CHECK(event(false).isEmpty());
        }
        result.response = 6;
        CHECK(event()["uses_consumed"].toInt() == 1);
        result.user.remaining = -1;
        CHECK(event()["uses_consumed"].toInt() == 0);
        CHECK(event()["remaining_uses"].isNull());
        result.user.id.clear();
        CHECK(event().isEmpty());
        result.response = 4;
        CHECK(!event().isEmpty());
        result.locked=true;result.lockedUntil=1700000060000;result.lockoutLevel=2;
        CHECK(event()["lockout"].toBool());
        CHECK(event()["locked_until"].toLongLong()==1700000060000);
        CHECK(event()["lockout_level"].toInt()==2);
        CHECK(event()["action"]=="invalid_code");
        CHECK(!event().contains("user_id"));
        CHECK(event(false).isEmpty());
        result.eventId.clear();
        CHECK(event().isEmpty());
        AlarmUsers::RestResult rest;
        rest.ok=true;rest.user=result.user;rest.user.id="synthetic-private-id";
        auto command=[&](bool managed=true, bool applied=true, QString operation="disarm") {
            return AlarmUsers::restEvent(managed,rest,"1",operation,"synthetic-rest-event",1700000000000,applied);
        };
        CHECK(command(false).isEmpty());
        CHECK(command()["result"]=="rejected");
        CHECK(command()["e"]=="alarm_command");
        CHECK(command()["source"]=="rest");
        CHECK(!command().contains("user_id"));
        CHECK(!command().contains("sensor_id"));
        CHECK(!QJsonDocument::fromVariant(command()).toJson().contains("synthetic-private"));
        rest.accepted=true;
        for(const char *op:{"disarm","arm_stay","arm_night","arm_away"}) {
            const auto request=command(true,true,op);
            CHECK(request["result"]=="accepted");
            CHECK(request["user_id"]=="synthetic-private-id");
            CHECK(request["uses_consumed"].toInt()==0);
            CHECK(!request.contains("remaining_uses"));
        }
        CHECK(command(true,false)["result"]=="failed");
        CHECK(command(true,true,"open").isEmpty());
        CHECK(!QJsonDocument::fromVariant(command()).toJson().contains("synthetic-private-hash"));
        rest.ok=false;CHECK(command().isEmpty());
        std::cout << "PASS: " << checks << " access-event checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

