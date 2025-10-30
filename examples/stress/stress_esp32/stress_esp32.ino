// Stress test for ESP32: repeatedly allocate and free Str and enqueue/dequeue
// RequestData to validate there is no heap leak.
//
// Flash this to an ESP32 and open the serial monitor at 115200.

#include "DLNA.h"
#include <utility>
#include "basic/Queue.h"

using namespace tiny_dlna;

const int ITERATIONS = 2000;
const int PAYLOAD_SIZE = 172; // observed packet size in your logs

void printMem(const char* tag) {
#ifdef ESP32
  Serial.printf("%s freeHeap=%u freePsram=%u\n", tag, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
#else
  Serial.printf("%s freeHeap=N/A\n", tag);
#endif
}

// Generate a simple payload of 'A's of given length
static void genPayload(char* buf, int len) {
  for (int i = 0; i < len; ++i) buf[i] = 'A' + (i % 26);
  buf[len] = '\0';
}

// Test 1: allocate/destroy many Str objects
void testAllocateDestroy() {
  Serial.println("=== testAllocateDestroy ===");
  printMem("start");
  char payload[PAYLOAD_SIZE + 1];
  genPayload(payload, PAYLOAD_SIZE);

  for (int i = 0; i < ITERATIONS; ++i) {
    {
      // scoped Str so destructor runs each iteration
      Str s;
      s.copyFrom(payload, PAYLOAD_SIZE, PAYLOAD_SIZE);
      // optionally do some work
      if ((i & 127) == 0) {
        // force a small string operation
        Str t = s.substring(0, 10);
        (void)t.c_str();
      }
    }

    if ((i % 250) == 0) {
      printMem("iter");
      delay(10);
    }
  }
  printMem("end");
}

// Test 2: enqueue/dequeue RequestData into a Queue
void testQueueEnqueueDequeue() {
  Serial.println("=== testQueueEnqueueDequeue ===");
  printMem("start");
  Queue<RequestData> q;
  char payload[PAYLOAD_SIZE + 1];
  genPayload(payload, PAYLOAD_SIZE);

  // Enqueue with a bounded queue to avoid exhausting heap on small boards.
  const int QUEUE_CAP = 100;  // keep number of items in queue bounded
  for (int i = 0; i < ITERATIONS; ++i) {
    RequestData r;
    r.peer.port = 1900;
    r.data.copyFrom(payload, PAYLOAD_SIZE, PAYLOAD_SIZE);
  bool ok = q.enqueue(std::move(r));
    if (!ok) {
      Serial.printf("enqueue failed at %d\n", i);
      break;
    }
    // keep queue bounded
    if ((int)q.size() > QUEUE_CAP) {
      RequestData tmp;
      q.dequeue(tmp);
    }
    if ((i % 250) == 0) printMem("enqueue");
  }

  // drain remaining items
  RequestData tmp;
  int deq = 0;
  while (q.dequeue(tmp)) {
    deq++;
    if ((deq % 250) == 0) printMem("dequeue");
  }
  Serial.printf("dequeued=%d\n", deq);
  printMem("end");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  Serial.println("Stress test starting...");
  delay(200);

  testAllocateDestroy();
  delay(200);
  testQueueEnqueueDequeue();

  Serial.println("Stress tests finished. Repeat in loop().");
}

void loop() {
  // repeat tests every 30s so you can observe long-term stability
  delay(30000);
  testAllocateDestroy();
  delay(200);
  testQueueEnqueueDequeue();
}
