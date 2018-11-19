#include "stdio.h"
#include "math.h"
#include "vortrack.h"

extern void vor(float S);
int interval=1;

static void gen(float rad){

double phase=0;
double phasefm=0;
double fmfreq,pfmfreq;
double S;
int n;

const double P=0.5;

printf("Test %f\n",180.0*rad/M_PI);

for(n=0;n<5*FSINT;n++) {

	S=P+0.3*P*cos(phase);

	fmfreq=9960+480*cos(phase+rad);

	phasefm+=M_PI*(fmfreq+pfmfreq)/FSINT;	
	if(phasefm>M_PI) phasefm -=2.0*M_PI;
	if(phasefm<-M_PI) phasefm +=2.0*M_PI;
	pfmfreq=fmfreq;

	S+=0.2*P*cos(phasefm);

	vor(S);

	phase+=2.0*M_PI*30.0/FSINT;
	if(phase>M_PI) phase -=2.0*M_PI;
}

}

int main(int argc, char **argrv)
{

gen(0);
gen(M_PI/4);
gen(M_PI/2);
gen(3.0*M_PI/4);
gen(M_PI);
gen(5.0*M_PI/4);
gen(3.0*M_PI/2);
gen(7.0*M_PI/4);
gen(2.0*M_PI);
gen(-M_PI/4);
gen(-M_PI/2);
gen(-3.0*M_PI/4);
gen(-M_PI);
gen(-5.0*M_PI/4);
gen(-3.0*M_PI/2);
gen(-7.0*M_PI/4);

}

