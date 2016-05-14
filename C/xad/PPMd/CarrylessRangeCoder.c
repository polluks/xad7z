#include "CarrylessRangeCoder.h"

void InitializeRangeCoder(CarrylessRangeCoder *self, ILookInStream *input,BOOL uselow,int bottom)
{
	UBYTE buffer[1];
	size_t size = 1;

	self->input=input;
	self->low=0;
	self->code=0;
	self->range=0xffffffff;
	self->uselow=uselow;
	self->bottom=bottom;

	for(int i=0;i<4;i++)
	{
		LookInStream_Read(input,&buffer,size);
		self->code=(self->code<<8)|buffer[0];
	}
}



uint32_t RangeCoderCurrentCount(CarrylessRangeCoder *self,uint32_t scale)
{
	self->range/=scale;
	return (self->code-self->low)/self->range;
}

void RemoveRangeCoderSubRange(CarrylessRangeCoder *self,uint32_t lowcount,uint32_t highcount)
{
	if(self->uselow) self->low+=self->range*lowcount;
	else self->code-=self->range*lowcount;

	self->range*=highcount-lowcount;

	NormalizeRangeCoder(self);
}


int NextSymbolFromRangeCoder(CarrylessRangeCoder *self,uint32_t *freqtable,int numfreq)
{
	uint32_t totalfreq=0;
	for(int i=0;i<numfreq;i++) totalfreq+=freqtable[i];

	uint32_t tmp=RangeCoderCurrentCount(self,totalfreq);

	uint32_t cumulativefreq=0;
	uint32_t n=0;
	while(n<numfreq-1&&cumulativefreq+freqtable[n]<=tmp) cumulativefreq+=freqtable[n++];

	RemoveRangeCoderSubRange(self,cumulativefreq,cumulativefreq+freqtable[n]);

	return n;
}

int NextBitFromRangeCoder(CarrylessRangeCoder *self)
{
	int bit=RangeCoderCurrentCount(self,2);

	if(bit==0) RemoveRangeCoderSubRange(self,0,1);
	else RemoveRangeCoderSubRange(self,1,2);

	return bit;
}

int NextWeightedBitFromRangeCoder(CarrylessRangeCoder *self,int weight,int size)
{
	uint32_t val=RangeCoderCurrentCount(self,size);

	int bit;
	if(val<weight) // <= ?
	{
		bit=0;
		RemoveRangeCoderSubRange(self,0,weight);
	}
	else
	{
		bit=1;
		RemoveRangeCoderSubRange(self,weight,size);
	}

	return bit;
}

int NextWeightedBitFromRangeCoder2(CarrylessRangeCoder *self,int weight,int shift)
{
	uint32_t threshold=(self->range>>shift)*weight;

	int bit;
	if(self->code<threshold) // <= ?
	{
		bit=0;
		self->range=threshold;
	}
	else
	{
		bit=1;
		self->range-=threshold;
		self->code-=threshold;
	}

	NormalizeRangeCoder(self);

	return bit;
}


void NormalizeRangeCoder(CarrylessRangeCoder *self)
{
	UBYTE buffer[1];
	size_t size = 1;

	for(;;)
	{
		if( (self->low^(self->low+self->range))>=0x1000000 )
		{
			if(self->range>=self->bottom) break;
			else self->range=-self->low&(self->bottom-1);
		}

		LookInStream_Read(self->input,&buffer,size);
		self->code=(self->code<<8) | buffer[0];
		self->range<<=8;
		self->low<<=8;
	}
}




/*int NextSymbolFromRangeCoderCumulative(CarrylessRangeCoder *self,uint32_t *cumulativetable,int stride)
{
	uint32_t totalfreq=*cumulativetable;
	cumulativetable=(uint32_t *)((uint8_t *)cumulativetable+stride);

	self->range/=totalfreq;
	uint32_t tmp=(self->code-self->low)/self->range;

	uint32_t n=0,curr;
	do
	{
		curr=*cumulativetable;
		cumulativetable=(uint32_t *)((uint8_t *)cumulativetable+stride);
		n++;
	} while(tmp<curr);

	cumulativetable=(uint32_t *)((uint8_t *)cumulativetable-2*stride);
	uint32_t prev=*cumulativetable;

	self->low+=self->range*curr;
	self->range*=prev-curr;

	NormalizeRangeCoder(self);

	return n-1;
}*/
