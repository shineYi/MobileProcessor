#include <stdio.h>
#include <stdlib.h>
//#include <stdbool.h>

#define CLINESIZE 16
#define MAXWAY 4
#define CSIZE 1024*2
#define MAXIDX CSIZE/(MAXWAY*CLINESIZE)

int Memory[1024*2*1024] = {0, };
int R[32] = {0, };
int PC = 0;
int EndPC = 0;
int cycle = 0;
unsigned int temp = 0;


//구조체 생성

typedef struct {

	unsigned int tag;
	unsigned int valid;
	unsigned int sca;
	unsigned int dirty;
	unsigned int data[CLINESIZE/sizeof(int)]; 
	int oldest;

} cacheline;

typedef struct {

        int Jtype, branch, memop;
        int regUp;

} ctrl_sig;

typedef struct {

         int nextPC;
         unsigned int inst;
         int cycle;
         int door;

} if_latch;

typedef struct {

         int nextPC;
         int door;
         int cycle;
         unsigned int opcode;
         unsigned int rs, rt, rd;
         unsigned int shamt, funct;
         short SEI;
         unsigned int imm;
         unsigned int JA;
         unsigned int BAddr;
         int val1, val2;

} id_latch;

typedef struct {

         int nextPC;
         int door;
         unsigned int opcode;
         unsigned int rs, rt, rd;
         int cycle;
         int result;
         int wreg;
         int val1, val2;
         short SEI;
		 int imm;

} exe_latch;

typedef struct {

         int nextPC;
         int door;
         int cycle;
         unsigned int opcode;
         int result;
         int wreg;

} mem_latch;

typedef struct {

        int target;

} jumpfunct;

//함수 선언

void fetch();
void decode();
void execute();
void MemOp();
void WriteBack();
void updatelatch();		//pipeline

unsigned int mem_read(int addr);
void mem_write(int addr, int val);
int oldest[MAXIDX]={0, };
int	oldest_way(int idx);			//cache


//변수 초기화

jumpfunct jvar = {0, };
jumpfunct bvar = {0, };
ctrl_sig c_signal = {0, };

if_latch L1[2] = {0, };
id_latch L2[2] = {0, };
exe_latch L3[2] = {0, };
mem_latch L4[2] = {0, };  //latch input, output

cacheline cache[MAXWAY][MAXIDX]={0,};

int fv=0;
int dv=0;
int ev=0;
int mv=0;
int wv=0;

//-------------------------------------------------------------------------initialize


int main(){

    int dstbuf = 0;

    R[31] = -1;
	R[29] = 1024*1024*2*4;

    FILE *fp = fopen("simple.bin", "rb");

         while(fread(&dstbuf, sizeof(int), 1, fp)){

         Memory[PC/4]  = ((dstbuf>>24)&0xff) |
                         ((dstbuf<<8)&0xff0000) |
                         ((dstbuf>>8)&0xff00) |
                         ((dstbuf<<24)&0xff000000);
		  PC+=4;

        } //swap

         PC = 0;
		 EndPC = 0;

         while(EndPC != -1){

                 printf("cycle: %d\n", cycle);
                 WriteBack();
                 MemOp();
				 execute();
				 decode();
				 fetch(); 
				
				 PC = L1[0].nextPC;	//PC value update
				 updatelatch();
                 cycle++;
                 printf("\n");

				 if(cycle%10==0)
				 {getchar();}
         }

         printf("Result: %d  [31]: %08x  PC: %08x\n", R[2], R[31], PC);

         return 0;

}


void fetch(){

        if(PC!=-1){

                if(c_signal.Jtype == 1){
                        PC = jvar.target;
                        PC-=4;
                        c_signal.Jtype = 0;
                }
				if(c_signal.branch == 1){
						PC = jvar.target;
						c_signal.branch = 0;
				}

                L1[0].nextPC = PC;
                L1[0].inst = mem_read(PC);
                L1[0].cycle = cycle;

              // printf("fetch cycle: %d", L1[0].cycle);
                printf("        %x:\t%08x\n", L1[0].nextPC, L1[0].inst);

                L1[0].nextPC+=4;
                L1[0].door = 1;
        }

        else{
			if(fv==0)
			{
				L1[0].nextPC = PC;
                printf("End fetch\n");
                L1[0].door = 2;
				fv=1;
				return ;
			}
        }
}


void decode(){
	             //    L2[1].nextPC = L1[2].nextPC;
         if(L1[1].door == 1){

                 L2[0].nextPC = L1[1].nextPC;
                 L2[0].cycle = L1[1].cycle;
				 L2[0].door = L1[1].door;

                unsigned int inst = L1[1].inst;
                unsigned int addr = (inst & 0x3ffffff);

               // printf("decode cycle: %d\n", L2[0].cycle);

                L2[0].opcode = ((inst >> 26)& 0x3f);
                L2[0].rs = ((inst >> 21)&0x1f);
                L2[0].rt = ((inst >> 16)&0x1f);
                L2[0].rd = ((inst >> 11)&0x1f);
                L2[0].shamt = ((inst >> 6)&0x1f);
                L2[0].funct = (inst & 0x3f); 
                L2[0].imm = (inst & 0xffff);
                L2[0].SEI = (short)(L2[0].imm);
                L2[0].BAddr = (((short)(L2[0].imm)<<2)&0xffffffff);
                L2[0].JA = (((L1[1].nextPC)&0xf000000) | (addr<<2));

                if(L2[0].opcode == 0 && L2[0].funct == 0x08){
                        c_signal.Jtype = 1;
                        jvar.target = R[L2[0].rs];
                        L2[0].door = false;
                }
                else if(L2[0].opcode == 2){
                        c_signal.Jtype = 1;
                        jvar.target = L2[0].JA+4;
                        L2[0].door = false;
                }
                else if(L2[0].opcode == 3){
                        c_signal.Jtype = 1;
						R[31] = L2[0].nextPC+8;
                        jvar.target = L2[0].JA;
                        L2[0].door = false;
                }

                else if(L3[1].opcode == 0x00){
                        if(L2[0].rs == L3[1].rd)
                                L2[0].val1 = L3[1].result;
                        else if(L2[0].rt == L3[1].rd)
                                L2[0].val2 = L3[1].result;
					}
		}

         else if(L1[1].door == 2){
			 if(dv==0)
			{
                L2[0].nextPC = L1[1].nextPC;
                printf("End Decode\n");
                L2[0].door = 2;
				dv=1;
			 }
        }
}


void execute(){

         if(L2[1].door == 1){

                 L3[0].cycle = L2[1].cycle;
                 L3[0].door = L2[1].door;
                 L3[0].nextPC = L2[1].nextPC;
                 L3[0].opcode = L2[1].opcode;
				 L3[0].SEI = L2[1].SEI;
				 L3[0].imm = L2[1].imm;

                 L3[0].rs = L2[1].rs;
                 L3[0].rt = L2[1].rt;
                 L3[0].rd = L2[1].rd;
				
               L3[0].val1 = R[L3[0].rs];
			   L3[0].val2 = R[L3[0].rt];

			   //depen_check1(&L1, L); depen_check(in,out,W_in);
                // printf("execute cycle: %d\n", L3[0].cycle);

                 if(L2[1].opcode == 0x00) {

					    if(L3[0].rs == L4[0].wreg)
                                L3[0].val1 = L4[0].result;
                        else if(L3[0].rt == L4[0].wreg)
                                L3[0].val2 = L4[0].result;	//dependency

						 L3[0].wreg = L2[1].rd;
						 c_signal.regUp = 1;

                         switch(L2[1].funct){

                                case 0x20: L3[0].result = L3[0].val1 + L3[0].val2;
									printf("\t\tR-type: Execute add function\n"); break;

                                case 0x21: temp = L3[0].val1+L3[0].val2;
                                   L3[0].result = temp;
                                   printf("\t\tR-type: Execute addu function\n"); break;

                                case 0x24: L3[0].result = L3[0].val1&L3[0].val2;
                                    printf("\t\tR-type: Execute and function\n"); break;

                                case 0x27: L3[0].result = ~(L3[0].val1|L3[0].val2);
                                    printf("\t\tR-type: Execute nor function\n"); break;

                                case 0x25: L3[0].result = L3[0].val1|L3[0].val2;
									printf("\t\tR-type: Execute or function\n"); break;

                               case 0x2a: L3[0].result = (L3[0].val1<L3[0].val2)? 1:0;
                                   printf("\t\tR-type: Execute slt function\n"); break;

                                case 0x2b: temp = (L3[0].val1<L3[0].val2)? 1:0;
                                   L3[0].result = temp;
                                   printf("\t\tR-type: Execute sltu function\n"); break;

                                case 0x00: L3[0].result = (L3[0].val2<< L2[1].shamt);
                                   printf("\t\tR-type: Execute sll function\n"); break;

                                case 0x02: L3[0].result = (L3[0].val2>>L2[1].shamt);
                                   printf("\t\tR-type: Execute srl function\n"); break;

                                case 0x22: L3[0].result = L3[0].val1-L3[0].val2;
                                    printf("\t\tR-type: Execute sub function\n"); break;

                                case 0x23: temp = L3[0].val1-L3[0].val2;
                                    L3[0].result = temp;
                                   printf("\t\tR-type: Execute subu function\n"); break;
                        }
                 }


                else {          //I Type

						if(L3[0].rs == L4[0].wreg)
                                L3[0].val1 = L4[0].result;
                        else if(L3[0].rt == L4[0].wreg)
                                L3[0].val2 = L4[0].result;
								
                         L3[0].wreg = L2[1].rt;
						 c_signal.regUp = 1;

                         switch(L2[1].opcode){

				case 0x04: if(L3[0].val1 == L3[0].val2){
					c_signal.branch =1;
					jvar.target = (L3[0].nextPC)+L2[1].BAddr;
					L3[0].door = false;}
					   printf("\t\t%d %d I type: Execute beq function\n", L3[0].val1, L3[0].val2); break;

				case 0x05: if(L3[0].val1 != L3[0].val2){
					c_signal.branch =1;
					jvar.target = (L3[0].nextPC)+L2[1].BAddr;
					L3[0].door = false;}
					   printf("%d %d", L3[0].val1, L3[0].val2);
					   printf("\t\tI type: Execute bne function\n"); break;

                                case 0x0f: L3[0].result = (L2[1].imm<<16);
                                    printf("\t\tI type: Execute lui function\n"); break;

                                case 0x08: L3[0].result = L3[0].val1 + L2[1].SEI;
									c_signal.regUp = 1;
                                    printf("\t\tI-type: Execute addi function\tR[%d] = %08x\n", L3[0].wreg, L3[0].result); break;

                                case 0x09: temp = L3[0].val1 + L2[1].SEI;
                                   L3[0].result = temp;
                                   printf("\t\tI-type: Execute addiu\tR[%d] = %08x\n", L3[0].wreg, L3[0].result); break;

                                case 0x0c: L3[0].result = L3[0].val1;
                                   printf("\t\tI-type: Execute andi\tR[%d] = %08x\n", L2[1].rt, L3[0].result); break;

                                case 0x0d: L3[0].result = (L3[0].val1|L2[1].imm);
                                   printf("\t\tI-type: Execute ori\tR[%d] = %08x\n", L3[0].wreg, L3[0].result); break;

                                case 0x0a: L3[0].result = (L3[0].val1<L2[1].SEI)? 1:0;
                                   printf("\t\tI-type: Execute slti\tR[%d] = %08x\n", L3[0].wreg, L3[0].result); break;

                                case 0x0b: temp = (L3[0].val1<L2[1].SEI)? 1:0;
                                   L3[0].result = temp;
                                   printf("\t\tI-type: Execute sltiu\tR[%d] = %08x\n", L3[0].wreg, L3[0].result); break;

								   //memory function
                                case 0x38: printf("\t\tDoes Not Support sc function.\n"); break;

				case 0x23: L3[0].result = mem_read((L3[0].val1+L3[0].SEI)/4);		//load word -> dirty 개념 제대로 하고 cache로 변환
					c_signal.regUp = 1;
                                    printf("\t\tMemory Operation: Execute lw function %d \n", L3[0].result); break;

                                }  //switch
                 }   //Itype
         }  //if door is 1

        else if(L2[1].door == 2){
			if(ev==0)
			{
                L3[0].nextPC = L2[1].nextPC;
                printf("End Execute\n");
                L3[0].door = L2[1].door;
				ev=1;
			}
        }
} //execute close

void MemOp(){

        if(L3[1].door == 1){

                L4[0].nextPC = L3[1].nextPC;
                L4[0].cycle = L3[1].cycle;
                L4[0].door = L3[1].door;
		L4[0].result = L3[1].result;
		L4[0].wreg = L3[1].wreg;

		int addr = (L3[1].val1+L3[1].SEI)/4;
               // printf("Load/Store cycle: %d\n", L4[0].cycle);

		switch(L3[1].opcode){								

                                case 0x28: mem_write(addr, L3[1].val2&0xff);		//store byte
						L4[0].door = 0;
                                    printf("\t\tMemory Operation: Execute sb function\nMemory[%x] = %08x\n", (L3[1].val1+L3[1].SEI)/4, Memory[(L3[1].val1+L3[1].SEI)/4]); break;

                                case 0x29: mem_write(addr, L3[1].val2&0x7fff);		//store halfword
						L4[0].door = 0;
                                    printf("\t\tMemory Operation: Execute sh function\nMemory[%x] = %08x\n", (L3[1].val1+L3[1].SEI)/4, Memory[(L3[1].val1+L3[1].SEI)/4]); break;

                                case 0x2b: mem_write(addr, R[L3[1].rt]);			//store word
					printf("R[%x]:%08x", L3[1].rt, R[L3[1].rt]);
					L4[0].door = 0;
					printf("\t\tMemory Operation: Execute sw function\n"); break;

				case 0x30: L4[0].result = Memory[(R[L3[1].rs]+L3[1].SEI)/4];		//load linked
					c_signal.regUp =1;
					printf("\t\tMemory Operation: Execute ll function\n");
					break;
                               
                                case 0x24: temp = (Memory[addr]&0xff);		//load byte unsigned
					c_signal.regUp = 1;
                                    L4[0].result = temp;
					printf("\t\tMemory Operation: Execute lbu function\n"); break;

                                case 0x25: temp = (Memory[addr]&0xffff);		//load halfword unsigned
					c_signal.regUp = 1;
                                    L4[0].result = temp;
                                    printf("\t\tMemory Operation: Execute lhu function\n"); break;
				}
		}

        else if(L3[1].door == 2){
			if(mv==0)
			{
				L4[0].nextPC = L3[1].nextPC;
                printf("End Memory Operation\n");
                L4[0].door = L3[1].door;
				mv=1;
			}
        }
}


void WriteBack(){

        if(L4[1].door == 1){
               // printf("Write Back Cycle: %d\n", L4[1].cycle);
               {
					{
                        R[L4[1].wreg] = L4[1].result;
                        printf("R[%d] = %x\n", L4[1].wreg, L4[1].result);
						c_signal.regUp = 0;
                
					}
				}
        }
        else if(L4[1].door == 2){
			if(wv==0)
			{
                printf("End Write Back\n");
				wv=1;
			}
				
		}
		EndPC = L4[1].nextPC;
}


void updatelatch(){

 L1[1] = L1[0];
 L2[1] = L2[0];
 L3[1] = L3[0];
 L4[1] = L4[0];
}

unsigned int mem_read(int addr){

	if(addr<0) return 0;	//if PC=-1

	unsigned int tagaddr = addr&0xfffffe00;
	unsigned int idx = (addr&0x1f0)>>4;
	unsigned int offset = addr & 0xf;
	unsigned int startAddr = addr & 0xfffffff0;
	int way=0;
	int i;
	int Waddr;

	printf("PC is %x, start address is %x\n", addr, startAddr);

	for(way=0; way<MAXWAY; way++){

		if(cache[way][idx].valid == 0){		//cold miss
			cache[way][idx].valid = 1;
			cache[way][idx].tag = tagaddr;
			oldest[idx]++;
			cache[way][idx].oldest = oldest[idx];

            printf("index:%x, tag:%x, valid:%d, sca:%d\n data:\n", idx, tagaddr, cache[way][idx].valid, cache[way][idx].sca);

			for(i=0; i<CLINESIZE/4; i++){
				cache[way][idx].data[i] = Memory[(startAddr/4)+i];
				printf("  %08x", cache[way][idx].data[i]);
				if(i%3==0)
					printf("\n");
			}
			printf("return data: cache[%d][%d] = %08x\n", way, idx, cache[way][idx].data[offset>>2]);
			return cache[way][idx].data[offset>>2];
		}

		else if(cache[way][idx].valid == 1 && cache[way][idx].tag == tagaddr){		//hit
			cache[way][idx].sca = 1;
			oldest[idx]++;
			cache[way][idx].oldest = oldest[idx];
			return cache[way][idx].data[offset>>2];
		}
	}				//hit나 cold miss일 경우 여기서 빠져나가고 아니면 루프 계속 돌아서 way=maxway된 상태

	way = oldest_way(idx);	//oldest 함수 만들기

	while(1){

		if(cache[way][idx].sca == 1){		//second chance
			cache[way][idx].sca = 0;
			way++;
		}

		else if(cache[way][idx].sca == 0){		//conflict miss
			oldest[idx]++;
			cache[way][idx].oldest = oldest[idx];

			if(cache[way][idx].dirty==0){								//not dirty
				cache[way][idx].valid = 1;
				cache[way][idx].tag = tagaddr;
               			printf("index:%x, tag:%x, valid:%d, sca:%d\n data:\n", idx, tagaddr, cache[way][idx].valid, cache[way][idx].sca);

				for(i=0; i<CLINESIZE/4; i++){
					cache[way][idx].data[i] = Memory[(startAddr/4)+i];
					printf("  %08x", cache[way][idx].data[i]);
					if(i%3==0)
					printf("\n");
				}
				printf("return data: cache[%d][%d] = %08x\n", way,idx,cache[way][idx].data[offset>>2]);
				return cache[way][idx].data[offset>>2];
			}

			else{		//dirty    
				Waddr = ((cache[way][idx].tag)/idx)<<4;
				for(i=0; i<CLINESIZE/4; i++)
					Memory[Waddr+i] = cache[way][idx].data[i];		//캐시 데이터 메모리에 집어넣기
				printf("Write back the data to the Memory");

				cache[way][idx].dirty=0;
				cache[way][idx].tag = tagaddr;

				for(i=0; i<CLINESIZE/4; i++){					//새로운 데이터 받기
					cache[way][idx].data[i] = Memory[(startAddr/4)+i];
				}
				return cache[way][idx].data[offset>>2];
			}
		}
		if(way==MAXWAY)
			way=0;
	}
}//end function

void mem_write(int addr, int val){

	unsigned int tagaddr = addr&0xfffffe00;
	unsigned int idx = (addr&0x1f0)>>4;
	unsigned int offset = addr & 0xf;
	unsigned int startAddr = addr & 0xfffffff0;
	int way;
	int i;
	int Waddr;

	//printf("MW val:%08x", val);
	for(way=0; way<MAXWAY; way++){

		if(cache[way][idx].valid == 0){		//cold miss
			cache[way][idx].valid = 1;
			cache[way][idx].dirty = 1;
			cache[way][idx].tag = tagaddr;
			oldest[idx]++;
			cache[way][idx].oldest = oldest[idx];
			printf("now chche[%d][%d] vlaue ==  %x , now data = %x check !!!\n\n",way,idx,val,cache[way][idx].data[offset/4]);
			for(i=0; i<CLINESIZE/4; i++){
				cache[way][idx].data[i] = Memory[(startAddr/4)+i];
			}

			cache[way][idx].data[offset>>2] = val;
			printf("Changed cache data: cache[%d][%d] = %08x\n", way,idx, cache[way][idx].data[offset>>2]);
			return;
		}

		else if(cache[way][idx].valid == 1 && cache[way][idx].tag == tagaddr){		//hit
			cache[way][idx].sca = 1;
			oldest[idx]++;
			cache[way][idx].oldest = oldest[idx];
			cache[way][idx].dirty = 1;
			cache[way][idx].data[offset>>2] = val;
			printf("Changed cache data: cache[%d][%d] = %08x\n", way, idx, cache[way][idx].data[offset>>2]);
			return;
		}
	}

	way = oldest_way(idx);	//oldest부터 시작 ***********oldest 함수 만들기

	while(1){

		if(cache[way][idx].sca == 1)		//second chance
			cache[way][idx].sca = 0;

		else if(cache[way][idx].sca == 0){		//conflict miss
			oldest[idx]++;
			cache[way][idx].oldest = oldest[idx];
			if(cache[way][idx].dirty==1){		//dirty    mem_write하고 다시 보기
				Waddr = ((cache[way][idx].tag)/idx)<<4;
				for(i=0; i<CLINESIZE/4; i++)
					Memory[Waddr+i] = cache[way][idx].data[i];
				printf("Write back the data to the Memory");

				for(i=0; i<CLINESIZE/4; i++){
					cache[way][idx].data[i] = Memory[(startAddr/4)+i];
				}
				cache[way][idx].data[offset>>2] = val;
				printf("Changed cache data: cache[%d][%d] = %08x\n", way, idx, cache[way][idx].data[offset>>2]);

				cache[way][idx].tag = tagaddr;
				return;
			}

			else{								//not dirty
				cache[way][idx].tag = tagaddr;

				for(i=0; i<CLINESIZE/4; i++){
					cache[way][idx].data[i] = Memory[(startAddr/4)+i];
				}
				cache[way][idx].data[offset>>2] = val;
				printf("Changed cache data: cache[%d][%d] = %08x\n", way, idx, cache[way][idx].data[offset>>2]);
				return;
			}
			cache[way][idx].dirty=1;
		}

		way++;
		if(way==MAXWAY)
			way=0;
	}
}

int oldest_way(int idx){
	int min;
	int old_way=0;
	int i;
	for(i=0; i<MAXWAY; i++){
		if(cache[i][idx].oldest<min){
			min=cache[i][idx].oldest;
			old_way = i;
		}
	}
	return old_way;
}
