// Copyright (C) 2010  Benoit Sigoure
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <linux/futex.h>
#include <string.h>
#include <sys/mman.h>
static inline unsigned long rdtsc(void)
{
        unsigned long low, high;

        asm volatile("rdtsc" : "=a" (low), "=d" (high));

        return ((low) | (high) << 32);
}

int main(void) {
  const int iterations = 500000;
  unsigned long long *results = malloc(sizeof(unsigned long long)*iterations);
  memset(results,0,sizeof(long long unsigned)*iterations);
  int ret= mlock(results,sizeof(long long unsigned)*iterations);
  double total=0.0;
  unsigned long long start, stop;
  const int shm_id = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666);
  const pid_t other = fork();
  int* futex = shmat(shm_id, NULL, 0);
  *futex = 0xA;
  if (other == 0) {
    start = rdtsc();
    for (int i = 0; i < iterations; i++) {
      sched_yield();
      while (syscall(SYS_futex, futex, FUTEX_WAIT, 0xA, NULL, NULL, 42)) {
        // retry
        sched_yield();
      }
      *futex = 0xB;
      while (!syscall(SYS_futex, futex, FUTEX_WAKE, 1, NULL, NULL, 42)) {
        // retry
        sched_yield();
      }
      stop = rdtsc();
      results[i]= stop-start;
      start = stop;
    }
    return 0;
  }
  start = rdtsc();
  for (int i = 0; i < iterations; i++) {
    *futex = 0xA;
    while (!syscall(SYS_futex, futex, FUTEX_WAKE, 1, NULL, NULL, 42)) {
      // retry
      sched_yield();
    }
    sched_yield();
    while (syscall(SYS_futex, futex, FUTEX_WAIT, 0xB, NULL, NULL, 42)) {
      // retry
      sched_yield();
    }
    stop = rdtsc();
    results[i]= stop-start;
    start = stop;
  }
  for (int i = 0; i < iterations; i++) {
	  printf("%lld\n",results[i]);
          //total+=results[i];
  }
  const int nswitches = iterations << 2;
//  printf("%i process context switches in %lfns (%.1fns/ctxsw)\n",
//         nswitches, total/2.1, ((total/2.1) / (float) nswitches));
  wait(futex);
  return 0;
}
