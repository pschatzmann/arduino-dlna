#pragma once

#include "Arduino.h"
#include "Basic/List.h"
#include "../Server/HttpLogger.h"

namespace tiny_dlna {

/// Information for each scheduled ticker request
struct TickerEntry {
    long repeatMs;
    long startMs= -1l; 
    long endMs = -1l;
    void(*callback)(void*);
    void *ctx;
    int id;
};

const long UNDEFINED_SCHEDULE = -1;
List<TickerEntry*> ticker_entries;
int TickerId;

/**
 * @brief A Simple Ticker which executes a callback function in defined
 * intervalls.
 * 
 */
class Ticker {

    public:
        Ticker(int cleanupSchedule=60000){
            schedule(cleanupSchedule,cleanup);
        }

        ~Ticker(){
            stopAll();
            ticker_entries.clear();
        }

         /// Schedule and starts the execution of the callback
        int schedule(long repeatMs, void(*callback)(void*), long startMs= UNDEFINED_SCHEDULE, long endMs = UNDEFINED_SCHEDULE, void* ctx = nullptr ) {
            long now = millis();
            TickerEntry *entry = new TickerEntry();
            entry->repeatMs = repeatMs;
            entry->startMs = startMs != UNDEFINED_SCHEDULE ? startMs : now;
            entry->endMs = endMs != UNDEFINED_SCHEDULE ? endMs : UNDEFINED_SCHEDULE;
            entry->callback = callback;
            entry->ctx = ctx;
            entry->id = TickerId++;
            ticker_entries.push_back(entry);
            return entry->id;
        }

        /// stops the execution of the callback by setting the end time;
        void stop(int id){
            for(auto it = ticker_entries.begin(); it != ticker_entries.end(); ++it) {
                TickerEntry *entry = *it;
                if (entry!=nullptr && entry->id==id){
                    entry->endMs = millis();
                    return;
                }
            }
        }

         /// stops the execution of the callback by setting the end time;
        void stopAll(){
            for(auto it = ticker_entries.begin(); it != ticker_entries.end(); ++it) {
                TickerEntry *entry = *it;
                entry->endMs = millis();
            }
        }


        /// please call this method in your loop()
        void doLoop() {
            long now = millis();
            for(auto it = ticker_entries.begin(); it != ticker_entries.end(); ++it) {
                TickerEntry *entry = *it;
                if (now >= entry->startMs 
                && (entry->endMs >= now || entry->endMs==UNDEFINED_SCHEDULE)){
                    // execute callback
                    entry->callback(entry->ctx);
                    // reschedule to new time
                    entry->startMs = now + entry->repeatMs;
                } 
            }
        }

    protected:

        // protected methods
        static void cleanup(void*) {
            HttpLogger.log(Info,"Ticker %s","cleanup");
            long count=0;
            long now = millis();
            for (int pos=ticker_entries.size()-1; pos>=0; pos--) {
                TickerEntry *entry = ticker_entries[pos];
                if (entry->endMs != UNDEFINED_SCHEDULE && entry->endMs < now ){
                    ticker_entries.erase(ticker_entries.begin()+pos);
                    count++;
                }
            }

            HttpLogger.log(Info,"Ticker cleaned up %d ticker_entries", count);
        }

};

}

