/**
 * @file test_listlockfree_rtos.ino
 * @brief Test ListLockFree with RTOS tasks (FreeRTOS/ESP32)
 *
 * This example demonstrates concurrent push and pop operations
 * on ListLockFree from multiple RTOS tasks.
 *
 * Hardware: ESP32 or compatible (with FreeRTOS)
 */

#include <Arduino.h>

#include "dlna.h"
#include "concurrency/ListLockFree.h"

using namespace tiny_dlna;

ListLockFree<int> list;
volatile int produced_count[2] = {0, 0};
volatile int consumed_count[2] = {0, 0};
volatile bool producer_done[2] = {false, false};
volatile bool consumer_done[2] = {false, false};
volatile int consumed_values[2][1000];
volatile int consumed_total = 0;
volatile bool validation_done = false;

void producerTask(void* param) {
  int idx = ((int)param == 1000) ? 0 : 1;
  int base = (int)param;
  for (int i = 0; i < 1000; ++i) {
    list.push_back(base + i);
    produced_count[idx]++;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  producer_done[idx] = true;
  vTaskDelete(NULL);
}

void consumerTask(void* param) {
  int idx = ((int)param == 1) ? 0 : 1;
  int value;
  int count = 0;
  while (count < 1000) {
    if (list.pop_front(value)) {
      Serial.print("Consumer ");
      Serial.print((int)param);
      Serial.print(" got: ");
      Serial.println(value);
      consumed_values[idx][count] = value;
      ++count;
      consumed_count[idx]++;
      ++consumed_total;
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  consumer_done[idx] = true;
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nListLockFree RTOS test");

  // Start two producers and two consumers
  xTaskCreatePinnedToCore(producerTask, "Producer1", 2048, (void*)1000, 1, NULL,
                          0);
  xTaskCreatePinnedToCore(producerTask, "Producer2", 2048, (void*)2000, 1, NULL,
                          1);
  xTaskCreatePinnedToCore(consumerTask, "Consumer1", 2048, (void*)1, 1, NULL,
                          0);
  xTaskCreatePinnedToCore(consumerTask, "Consumer2", 2048, (void*)2, 1, NULL,
                          1);
}

void validateResults() {
  // Wait for all tasks to finish
  if (!(producer_done[0] && producer_done[1] && consumer_done[0] &&
        consumer_done[1]))
    return;
  if (validation_done) return;
  validation_done = true;

  // Check produced/consumed counts
  int total_produced = produced_count[0] + produced_count[1];
  int total_consumed = consumed_count[0] + consumed_count[1];
  Serial.println();
  Serial.print("Produced: ");
  Serial.print(total_produced);
  Serial.print(" (P1: ");
  Serial.print(produced_count[0]);
  Serial.print(", P2: ");
  Serial.print(produced_count[1]);
  Serial.println(")");
  Serial.print("Consumed: ");
  Serial.print(total_consumed);
  Serial.print(" (C1: ");
  Serial.print(consumed_count[0]);
  Serial.print(", C2: ");
  Serial.print(consumed_count[1]);
  Serial.println(")");

  // Check for duplicates and missing values (only for produced ranges)
  bool seen[2000] = {0};  // 0-999: 1000-1999, 1000-1999: 2000-2999
  int dup_count = 0, miss_count = 0;
  for (int c = 0; c < 2; ++c) {
    for (int i = 0; i < 1000; ++i) {
      int v = consumed_values[c][i];
      if (v >= 1000 && v < 2000) {
        if (seen[v - 1000]) dup_count++;
        seen[v - 1000] = true;
      } else if (v >= 2000 && v < 3000) {
        if (seen[1000 + v - 2000]) dup_count++;
        seen[1000 + v - 2000] = true;
      }
    }
  }
  // Check for missing
  for (int v = 0; v < 1000; ++v) {
    if (!seen[v]) miss_count++;
    if (!seen[1000 + v]) miss_count++;
  }

  Serial.print("Duplicates: ");
  Serial.println(dup_count);
  Serial.print("Missing: ");
  Serial.println(miss_count);
  Serial.print("Final list size: ");
  Serial.println((unsigned)list.size());
  if (dup_count == 0 && miss_count == 0 && list.size() == 0) {
    Serial.println(
        "\nTEST PASSED: All values transferred correctly, no duplicates, no "
        "missing, list empty.");
  } else {
    Serial.println("\nTEST FAILED: Data integrity issue detected.");
  }
  while (true);
}

void loop() {
  // Print list size every second
  Serial.print("List size: ");
  Serial.println((unsigned)list.size());
  validateResults();
  delay(1000);
}
