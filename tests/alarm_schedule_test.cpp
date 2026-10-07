#include "alarm_user_schedule.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
using AlarmUsers::checkSchedule;
static int checks=0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__)); } while(0)
static qint64 at(const char *s) { return QDateTime::fromString(s,Qt::ISODate).toMSecsSinceEpoch(); }
static std::string schedule(const char *zone, const char *windows, const char *expires="null", const char *start="null") {
    return std::string("{\"timezone\":\"")+zone+"\",\"not_before\":"+start+",\"expires_at\":"+expires+",\"windows\":"+windows+"}";
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        CHECK(checkSchedule("",0)==1);
        CHECK(checkSchedule("broken",0)==-1);
        CHECK(checkSchedule(schedule("Not/A_Zone","[]"),0)==-1);
        auto daily=schedule("UTC","[]");
        CHECK(checkSchedule(daily,0)==1);
        CHECK(checkSchedule(daily,at("2026-09-21T10:00:00Z"))==1);
        CHECK(checkSchedule(daily,123)==-1);
        CHECK(checkSchedule(daily,-1)==-1);
        CHECK(checkSchedule(daily,4102444800001LL)==-1);
        auto expiry=schedule("UTC","[]","1790000000000","1789990000000");
        CHECK(checkSchedule(expiry,1789989999999LL)==0);
        CHECK(checkSchedule(expiry,1789990000000LL)==1);
        CHECK(checkSchedule(expiry,1789999999999LL)==1);
        CHECK(checkSchedule(expiry,1790000000000LL)==0);
        CHECK(checkSchedule(schedule("UTC","[]","1789990000000","1790000000000"),0)==-1);
        auto weekly=schedule("America/Los_Angeles","[{\"day\":1,\"start\":540,\"end\":1020}]");
        CHECK(checkSchedule(weekly,at("2026-09-21T15:59:59Z"))==0);
        CHECK(checkSchedule(weekly,at("2026-09-21T16:00:00Z"))==1);
        CHECK(checkSchedule(weekly,at("2026-09-22T00:00:00Z"))==0);
        CHECK(checkSchedule(weekly,at("2026-09-22T16:00:00Z"))==0);
        CHECK(checkSchedule(weekly,at("2026-12-21T17:00:00Z"))==1); // standard time
        // Fall-back repeats the allowed local hour; BOTH occurrences are allowed.
        auto fall=schedule("America/Los_Angeles","[{\"day\":7,\"start\":60,\"end\":120}]");
        CHECK(checkSchedule(fall,at("2026-11-01T08:30:00Z"))==1);
        CHECK(checkSchedule(fall,at("2026-11-01T09:30:00Z"))==1);
        CHECK(checkSchedule(fall,at("2026-11-01T10:00:00Z"))==0);
        // Spring-forward missing local minutes never occur; no catch-up window.
        auto spring=schedule("America/Los_Angeles","[{\"day\":7,\"start\":120,\"end\":180}]");
        CHECK(checkSchedule(spring,at("2026-03-08T09:59:59Z"))==0);
        CHECK(checkSchedule(spring,at("2026-03-08T10:00:00Z"))==0);
        CHECK(checkSchedule(schedule("UTC","[{\"day\":1,\"start\":0,\"end\":1440}]"),at("2026-09-21T23:59:59Z"))==1);
        for(const auto *windows:{"{}","[{}]","[{\"day\":0,\"start\":0,\"end\":1}]","[{\"day\":1,\"start\":1,\"end\":1}]","[{\"day\":1,\"start\":0,\"end\":1441}]","[{\"day\":1.5,\"start\":0,\"end\":1}]"})
            CHECK(checkSchedule(schedule("UTC",windows),0)==-1);
        std::cout<<"PASS: "<<checks<<" Qt schedule/timezone/DST checks\n";
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\n"; return 1; }
}
