/*
 * Copyright 2018 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

namespace CDMi {

class CriticalSection {
public:
    CriticalSection() {
        pthread_mutexattr_t structAttributes;

        // Create a recursive mutex for this process (no named version, use semaphore)
        if (pthread_mutexattr_init(&structAttributes) != 0) {
            // That will be the day, if this fails...
            assert(false);
        }
        else if (pthread_mutexattr_settype(&structAttributes, PTHREAD_MUTEX_RECURSIVE) != 0) {
            // That will be the day, if this fails...
            assert(false);
        }
        else if (pthread_mutex_init(&_lock, &structAttributes) != 0) {
            // That will be the day, if this fails...
            assert(false);
        }
    }

    ~CriticalSection() {
        if (pthread_mutex_destroy(&_lock) != 0) {
            printf("Probably trying to delete a used CriticalSection.\n");
        }
    }

public:
    inline void Lock() const
    {
      if (pthread_mutex_lock(&_lock) != 0) {
        printf("Probably creating a deadlock situation\n");
      }
    }
    inline void Unlock() const
    {
      if (pthread_mutex_unlock(&_lock) != 0) {
        printf("Probably does the calling thread not own this CCriticalSection.\n");
      }
    }

private:
    pthread_mutex_t _lock;
};

} // namespace CDMi
