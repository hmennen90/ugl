#include <stdio.h>
#include <stdint.h>
#include <time.h>
static double now_ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
int main(void){
  volatile float pv[3]={0.1f,0.7f,1.3f}, qv[3]={0.3f,0.9f,2.1f};
  float p[3]={pv[0],pv[1],pv[2]}, q[3]={qv[0],qv[1],qv[2]};
  const int N=3000000; double s=0.0; double t0=now_ms();
  for(int i=0;i<N;i++){ volatile float a=p[0]*q[0]; (void)a; s += (double)p[0]*(double)q[0] + (double)p[1]*(double)q[1] + (double)p[2]*(double)q[2]; p[0]=pv[0]; }
  double t1=now_ms(); printf("C ref dot    %8.2f ms  result %.6f\n", t1-t0, s);
  float pos[3]={0,0,0}; const float vel[3]={1,2,3}; volatile float dt=0.016f; t0=now_ms();
  for(int i=0;i<N;i++){ for(int k=0;k<3;k++) pos[k] += vel[k]*dt; }
  t1=now_ms(); printf("C ref update %8.2f ms  result %.6f %.6f %.6f\n", t1-t0, pos[0],pos[1],pos[2]);
  return 0; }
