/**
 * Semaphores and Shared Memory
 * Written by - Ryan Seys
 * Date - October 13, 2012
 * There is no deadlock in this application. I have implemented such a solution that I believe will result in no dead lock under regular conditions.
 * The execution order is as follows:
 * - Create 6 semaphores, 1 for each of the 5 files (each initialized to 1), and 1 "critical semaphore" to restrict file access to 4 processes at a time (it is initialized to 4)
 * - Create 5 integers in shared memory, initialize them all as zero (files are free, not busy)
 * - Create 5 child process and each do this:
 *   - Acquire preliminary access to even getting to the files through the "critical semaphore"
 *   - Acquire access to the first database that this process needs through the semaphore associated with that resource
 *   - Acquire access to the second database that this process needs through the semaphore associated with that resource
 *   - Once acquired all three of these things, write a 1 to the shared memory for both of the resources (to say file busy)
 *   - Open the file, write to the file, wait a second to simulate some more operations on the file, and close the file.
 *   - Write a zero to the shared memory locations for both of the resources (to say the file is no longer busy)
 *   - Release all semaphores acquired at the beginning of the transaction.
 *   - exit(0) back to the main function
 * - The main function will then clean up shared memory and semaphores and exit appropriately once all 5 child processes have finished executing.
*/

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <unistd.h>

using namespace std;

#define SEM_MODE 0644 /* rw-r--r-- */
#define SHM_MODE 0666 /* rw-rw-rw- */

/*This declaration is *MISSING* in many Unix environments.
 *It should be in the  file but often is not! If you
 *receive a duplicate definition error for semun then comment out
 *the union definition.
 */

union semun
{
  int val;
  struct semid_ds *buf;
  ushort  *array;
  struct seminfo *__buf;
};

// prototypes
void open_and_write(int, int **, int);
void print_sem_val(int, int);
void init_sem(int, int, int);
void acquire_resource(int, int);
void release_resource(int, int);
int create_shared_mem_id();
int destroy_mem_segment(int);
int create_semaphore_set(int);
int * get_pointer_to_mem(int);

// When this debug flag is set to true,
// it will print out extra console messages
// regarding the low level actions that occur
// in the system with shared memory and semaphores
bool debug = false;

int main() {
  int PROC_COUNT = 5;
  int * shm_ary[5]; //array of pointers to shared mem sections
  int shmIds[5];
  int pid;


  if(debug) cout << "Parent process started" << endl;

  //create 5 semaphores (1 per file)
  //create a 6th semaphore to solve problem of deadlock if any would have occurred
  int semSet = create_semaphore_set(6);
  if(debug) cout << "Created semaphore set: " << semSet << endl;

  //initialize the first 5 semaphores to 1
  for(int sem = 0; sem < 5; sem++) {
    init_sem(semSet, sem, 1);

    int id; //pointer to address
    if((id = create_shared_mem_id()) != -1) {
      shmIds[sem] = id;
      shm_ary[sem] = get_pointer_to_mem(id);
      *shm_ary[sem] = 0; //store 0 as the initial value
      if(debug) cout << "Added shared mem " << id << " to shm_ary[" << sem << "]" << endl;
    }
    else {
      cout << "Failed getting shared memory" << endl;
      exit(-1);
    }
  }

  // initialize the 6th semaphore to a value of 4 so that only 4 processes
  // have access to the 5 files at once. This will eliminate any
  // deadlock that could occur if all 5 processes had hold of one file
  // and was waiting for their other file to become free
  init_sem(semSet, 5, 4);

  for(int i=0; i < PROC_COUNT; i++) {
    pid = fork();
    if(pid < 0) {
      fprintf(stderr, "Fork Failed");
      exit(-1);
    }
    if (pid == 0) { /* child process */
      if(debug) cout << "Running child process " << getpid() << endl;
      open_and_write(semSet,shm_ary,i);
      exit(0);
    }
    else { /* parent process */
      continue;
    }
  }
  if(debug) cout << "Parent waiting for children to all finish" << endl;
  int j; // will hold the value of the pid of the finished child process
  while((j = wait(NULL)) != -1) {
    if(debug) cout << "Child " << j << " finished" << endl;
  }

  // Cleanup (destroy) all 5 shared memory segments
  for(int i = 0; i < 5; i++) {
    if(destroy_mem_segment(shmIds[i]) != -1) {
      if(debug) cout << "Deleted memory segment with ID " << shmIds[i] << endl;
    }
    else {
      cout << "Error occurred destroying memory segment" << endl;
    }
  }

  // Remove all semaphores in the set of semaphores
  // (start at index 0 and remove all from then on)
  int semId;
  if(semId = (semctl(semSet, 0, IPC_RMID, 0)) == -1){
    perror("Semaphore was not removed.\n");
  }
  else{
    if(debug) cout << "Semaphores were removed succesfully" << endl;
  }

  // Done!
  if(debug) cout << "Parent process finished" << endl;
  exit(0);
}

// Opens a file, after acquiring the semaphore with that particular resource,
// then write to shared memory to doubly represent that the file is use
// This shared memory could later be used as a monitor for the access status
// of the file. (1 = used, 0 = free). After writing to memory, it will write to the files,
// and rewrite the shared memory to 0 (free), then release the semaphore to show the resource
// is now free for another process to use.
void open_and_write(int semSet, int ** shm_ary, int i) {
  //calculate the values of the resources required
  int sem1 = i;
  int sem2 = (i + 1 == 5) ? 0 : i + 1;

  ofstream db1;
  ofstream db2;
  const char * db1filename;
  const char * db2filename;
  const char * systemName;

  // Acquire the required resources to do the database transaction
  acquire_resource(semSet, 5); // get to be one of the four processes that can access files
  acquire_resource(semSet, sem1); //get access to database 1
  acquire_resource(semSet, sem2); // get access to database 2

  //Ensure that the shared memory values are set to the
  //
  if(*shm_ary[sem1] == 0) {
    if(debug) cout << "Writing 1 to shared memory space for resource " << sem1 << " (now busy)" << endl;
    *shm_ary[sem1] = 1; // write a 1 to show that the file is busy
  }
  else {
    cout << "ERROR: Resources are in use!" << endl;
    exit(-1);
  }
  if(*shm_ary[sem2] == 0) {
    if(debug) cout << "Writing 1 to shared memory space for resource " << sem2 << " (now busy)" << endl;
    *shm_ary[sem2] = 1; // write a 1 to show that the file is busy
  }
  else {
    cout << "ERROR: Resources are in use!" << endl;
    exit(-1);
  }
  //Switch through all the different systems and the databases they need
  switch(i) {
    //Courses System
    case 0:
      systemName = "Courses System";
      db1filename = "faculty.txt";
      db2filename = "students.txt";
    break;
    //GPA Computation System
    case 1:
      systemName = "GPA Computation System";
      db1filename = "students.txt";
      db2filename = "statistics.txt";
    break;
    //University Stats System
    case 2:
      systemName = "University Statistics System";
      db1filename = "statistics.txt";
      db2filename = "staff.txt";
    break;
    //Staff Management System
    case 3:
      systemName = "Staff Management System";
      db1filename = "staff.txt";
      db2filename = "salary.txt";
    break;
    //Faculty Payroll System
    case 4:
      systemName = "Faculty Payroll System";
      db1filename = "salary.txt";
      db2filename = "faculty.txt";
    break;
  }

  // open files once we have acquired both semaphores
  db1.open(db1filename, ofstream::out | ofstream::app);
  db2.open(db2filename, ofstream::out | ofstream::app);

  // do all work with databases in here while you have access
  cout << systemName << " (pid: " << getpid() << ") writing to " << db1filename << endl;
  db1 << "Being used by " << systemName << " (pid:" << getpid()  << ")" << endl;
  sleep(1); // sleep 1 second to simulate database action
  cout << systemName << " (pid: " << getpid() << ") writing to " << db2filename << endl;
  db2 << "Being used by " << systemName << " (pid:" << getpid()  << ")" << endl;
  sleep(1); // sleep 1 second to simulate database action
  db1 << "Free from the " << systemName << " (pid: " << getpid()  << ")" << endl;
  db2 << "Free from the " << systemName << " (pid: " << getpid()  << ")" << endl;

  //close resource, rewrite shared memory to 0, and release resource from semaphore
  db1.close();
  if(debug) cout << "Writing 0 to shared memory space for resource " << sem1 << " (now free)" << endl;
  *shm_ary[sem1] = 0; //set shared memory to 0 to show that that resource is available now
  release_resource(semSet, sem1); //release semaphore so another process can acquire it
  cout << systemName << " (pid: " << getpid() << ") freed up access to " << db1filename << endl;

  db2.close();
  if(debug) cout << "Writing 0 to shared memory space for resource " << sem2 << " (now free)" << endl;
  *shm_ary[sem2] = 0; //set shared memory to 0 to show that that resource is available now
  release_resource(semSet, sem2); //release semaphore so another process can acquire it
  cout << systemName << " (pid: " << getpid() << ") freed up access to " << db2filename << endl;
  release_resource(semSet, 5);
}

// Prints out the value of a semaphore to the standard output
void print_sem_val(int semSet, int semid) {
  int semVal = semctl(semSet, semid, GETVAL, 0);
  cout << "Semaphore " << semid << " value: " << semVal << endl;
}

// Initialize a semaphore with an integer value
// In the case of this application we set the value
// of the semaphore to 1 (for a binary semaphore)
void init_sem(int semSet, int semid, int value) {
  union semun sem_init;
  sem_init.val = value;
  semctl(semSet, semid, SETVAL, sem_init);
}

// Acquire the semaphore and print out its value
// This essentially decreases the value of the semaphore by 1
void acquire_resource(int semSet, int semid) {
  if(debug) {
    cout << "Acquiring semaphore " << semid << endl;
    print_sem_val(semSet, semid);
  }

  struct sembuf sem;
  sem.sem_num = semid;
  sem.sem_flg = SEM_UNDO;
  sem.sem_op = -1;
  semop(semSet,&sem,1);

  if(debug) {
    cout << "Semaphore " << semid << " acquired!" << endl;
    print_sem_val(semSet, semid);
  }
}

// Release the semaphore and print out its value
// This essentially increase the value of the semaphore by 1
void release_resource(int semSet, int semid) {
  if(debug) {
    cout << "Releasing semaphore " << semid << endl;
    print_sem_val(semSet, semid);
  }
  struct sembuf sem;
  sem.sem_num = semid;
  sem.sem_flg = SEM_UNDO;
  sem.sem_op = 1;
  semop(semSet,&sem,1);
  if(debug) {
    cout << "Semaphore " << semid << " released!" << endl;
    print_sem_val(semSet, semid);
  }
}

// Returns the id of the shared memory space
// Use get_pointer_to_mem to get a memory address
int create_shared_mem_id() {
  int shmId;
  if((shmId = shmget(IPC_PRIVATE, sizeof(int),SHM_MODE)) == -1) {
    perror("shmget error");
    return -1;
  }
  else {
    if(debug) cout << "Shared memory created with id: " << shmId << endl;
    return shmId;
  }
}

// Attaches shared memory id to a physical address
// Must pass in the shared memory id
int * get_pointer_to_mem(int shmId) {
  int * sharedVar;
  sharedVar = (int *) shmat(shmId,0,0);
  if(sharedVar == (int *) -1) {
    perror("shmat error");
    return NULL;
  }
  else {
    if(debug) cout << "Starting address of shared variable is: " << sharedVar << endl;
    return sharedVar;
  }
}

// Frees the memory segment associated with that shared memory ID
// This allows this memory to be re-allocated to other processes later
int destroy_mem_segment(int shmId) {
  if(shmctl(shmId,IPC_RMID, (struct shmid_ds *) 0 ) < 0) {
    perror("can't destroy segment");
    return -1;
  }
  return 0;
}

// Creates a set of semaphores of a size of the integer passed
// In this example we pass it a value of 5 to create 5 semaphores
int create_semaphore_set(int num_of_sems) {
  return semget(IPC_PRIVATE, num_of_sems, IPC_CREAT | SEM_MODE);
}
