// In xv6/trap.c

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

struct gatedesc idt[256];
extern uint vectors[];
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

void
trap(struct trapframe *tf)
{
  struct proc *curproc = myproc();

  if(tf->trapno == T_SYSCALL){
    if(curproc && curproc->killed) 
      exit();
    if(curproc == 0) 
      panic("syscall from no process");
    curproc->tf = tf;
    syscall();
    if(curproc && curproc->killed) 
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  
  case T_PGFLT: { // 페이지 폴트 처리 시작
    uint fault_addr;         // 폴트가 발생한 가상 주소를 저장할 변수
    char *mem;               // 새로 할당할 물리 페이지의 커널 가상 주소를 저장할 변수
    uint page_aligned_addr;  // 폴트 주소를 페이지 경계로 정렬한 주소를 저장할 변수

    fault_addr = rcr2(); // CR2 레지스터에서 폴트가 발생한 가상 주소를 읽어옴

    if(curproc == 0) { 
        cprintf("Page fault with no process context. fault_addr: 0x%x\n", fault_addr); 
        panic("T_PGFLT no proc"); 
    }
    if (fault_addr < KERNBASE && fault_addr < curproc->sz) {
        page_aligned_addr = PGROUNDDOWN(fault_addr); // 폴트 주소를 해당 페이지의 시작 주소로 정렬

        mem = kalloc(); // 새로운 물리 페이지를 할당
        if(mem == 0){ // 물리 메모리 할당 실패 시 (메모리 부족)
            cprintf("lazy page fault: kalloc out of memory. pid %d name %s, fault_addr 0x%x\n", 
                    curproc->pid, curproc->name, fault_addr);
            curproc->killed = 1; // 현재 프로세스를 종료하도록 플래그 설정
        } else { // 물리 메모리 할당 성공 시
            memset(mem, 0, PGSIZE); // 할당받은 물리 페이지를 0으로 초기화

            if(mappages(curproc->pgdir, (char*)page_aligned_addr, PGSIZE, V2P(mem), PTE_W|PTE_U|PTE_P) < 0){
                cprintf("lazy page fault: mappages failed. pid %d name %s, fault_addr 0x%x\n", 
                        curproc->pid, curproc->name, fault_addr);
                kfree(mem); // 매핑 실패 시 방금 할당한 물리 페이지 해제
                curproc->killed = 1; // 현재 프로세스를 종료하도록 플래그 설정
            } else {
                
                lcr3(V2P(curproc->pgdir)); 
            }
        }
    } else { 

        cprintf("pid %d %s: segmentation fault (trap %d err %d) on cpu %d "
                "eip 0x%x fault_addr 0x%x\n",
                curproc->pid, curproc->name, tf->trapno,
                tf->err, cpuid(), tf->eip, fault_addr);
        curproc->killed = 1; // 현재 프로세스를 종료하도록 플래그 설정
    }
    break; // T_PGFLT case 종료
  } // case T_PGFLT의 끝

  default:
    if(curproc == 0 || (tf->cs&3) == 0){
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x --kill proc\n",
            curproc->pid, curproc->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    curproc->killed = 1;
  }

  if(curproc && curproc->killed && (tf->cs&3) == DPL_USER)
    exit();

  if(curproc && curproc->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  if(curproc && curproc->killed && (tf->cs&3) == DPL_USER)
    exit();
}