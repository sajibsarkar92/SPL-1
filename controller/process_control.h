#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

void suspend_process(int pid);
void resume_process(int pid);
void terminate_process(int pid);
void force_kill(int pid); 

#endif