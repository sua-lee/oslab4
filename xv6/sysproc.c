#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

// 지연 할당(lazy allocation)을 위해 수정된 growproc 함수를 호출합니다.
// growproc 함수 자체는 proc.c에 있어야 합니다.
int
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc *curproc = myproc(); // myproc() 호출을 통해 현재 프로세스 정보를 가져옵니다.

  if(argint(0, &n) < 0) // 인자로 받은 n 값을 가져옵니다.
    return -1;
  addr = curproc->sz;    // 현재 프로세스의 크기를 addr에 저장합니다.
  if(growproc(n) < 0)    // growproc을 호출하여 프로세스 크기를 n만큼 변경합니다.
    return -1;
  return addr;           // 변경 전의 크기(break point)를 반환합니다.
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//new sys function added
int 
sys_printpt(void) {
    int pid;
    if (argint(0, &pid) < 0)
        return -1;
    printpt(pid);
    return 0;
}
//new sys function ended