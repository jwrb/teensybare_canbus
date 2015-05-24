#include "mk20d7.h"
#include "can.h"

static const int rxb=0; // mailbox number for rx
static const int txb=8; // mailbox number for tx

void CAN_Init(void)
{
	PORTA_PCR12=PORT_PCR_MUX(0x02);
	PORTA_PCR13=PORT_PCR_MUX(0x02);

	SIM_SCGC6|=SIM_SCGC6_FLEXCAN0_MASK;

	CAN0_CTRL1&=~CAN_CTRL1_CLKSRC_MASK;

	CAN0_MCR|=CAN_MCR_FRZ_MASK;
	CAN0_MCR&=~CAN_MCR_MDIS_MASK;
	while(CAN0_MCR&CAN_MCR_LPMACK_MASK);

	CAN0_MCR|=CAN_MCR_SOFTRST_MASK;
	while(CAN0_MCR&CAN_MCR_SOFTRST_MASK);

	while(!(CAN0_MCR&CAN_MCR_FRZACK_MASK));

	CAN0_MCR|=CAN_MCR_SRXDIS_MASK;
	CAN0_MCR|=CAN_MCR_RFEN_MASK;

//	CAN0_CTRL1=CAN_CTRL1_PROPSEG(2)|CAN_CTRL1_RJW(1)|CAN_CTRL1_PSEG1(7)|CAN_CTRL1_PSEG2(3)|CAN_CTRL1_PRESDIV(1); // 500KBPS
	CAN0_CTRL1=CAN_CTRL1_PROPSEG(2)|CAN_CTRL1_RJW(1)|CAN_CTRL1_PSEG1(7)|CAN_CTRL1_PSEG2(3)|CAN_CTRL1_PRESDIV(9); // 100KBPS
	CAN0_CTRL1&=~CAN_CTRL1_LPB_MASK;

	CAN0_RXIMR0=CAN_RXIMR_MI(0x1FFFFFFF);
	CAN0_RXMGMASK=0;
	CAN0_RXFGMASK=0;

	CAN0_MCR&=~CAN_MCR_HALT_MASK;
	while(CAN0_MCR&CAN_MCR_FRZACK_MASK);
	while(CAN0_MCR&CAN_MCR_NOTRDY_MASK);

	CAN0_CS(txb)=CAN_CS_CODE(0/*INACTIVE*/);
}

int CAN_ReadFrame(CAN_Frame_t *Frame)
{
	int i;

	while(!(CAN0_IFLAG1&CAN_IFLAG1_BUF5I_MASK))
		return 0;

	Frame->Length=(CAN0_CS(rxb)&CAN_CS_DLC_MASK)>>CAN_CS_DLC_SHIFT;

	if(CAN0_CS(rxb)&CAN_CS_IDE_MASK)
		Frame->MessageID=(CAN0_ID(rxb)&(CAN_ID_STD_MASK|CAN_ID_EXT_MASK))|CAN_MESSAGE_ID_EXT;
	else
		Frame->MessageID=(CAN0_ID(rxb)&CAN_ID_STD_MASK)>>CAN_ID_STD_SHIFT;

	for(i=0;i<Frame->Length;i++)
	{
		if((i&0xFC)==0)
			Frame->Data[i]=CAN0_WORD0(rxb)>>(24-((i&0x3)<<3));
		else
			Frame->Data[i]=CAN0_WORD1(rxb)>>(24-((i&0x3)<<3));
	}

	CAN0_IFLAG1=CAN_IFLAG1_BUF5I_MASK;

	return 1;
}

int CAN_SendFrame(CAN_Frame_t Frame)
{
	int i;

	while((CAN0_ESR1&(CAN_ESR1_RX_MASK|CAN_ESR1_TX_MASK))!=0x00);

	CAN0_CS(txb)=CAN_CS_CODE(0/*INACTIVE*/);

	if((Frame.MessageID&CAN_MESSAGE_ID_EXT)!=0x00)
	{
		CAN0_ID(txb)&=~(CAN_ID_STD_MASK|CAN_ID_EXT_MASK);
		CAN0_ID(txb)|=(Frame.MessageID&~CAN_MESSAGE_ID_EXT);
		CAN0_CS(txb)&=~CAN_CS_IDE_MASK;
		CAN0_CS(txb)|=(1<<CAN_CS_IDE_SHIFT);
	}
	else
	{
		CAN0_ID(txb)&=~CAN_ID_STD_MASK;
		CAN0_ID(txb)|=(Frame.MessageID<<CAN_ID_STD_SHIFT);
		CAN0_CS(txb)&=~CAN_CS_IDE_MASK;
		CAN0_CS(txb)|=(0<<CAN_CS_IDE_SHIFT);
	}

	for(i=0;i<Frame.Length;i++)
	{
		if((i&0xFC)==0)
			CAN0_WORD0(txb)=(CAN0_WORD0(txb)&~(0xFF<<(24-((i&0x3)<<3))))|(Frame.Data[i]<<(24-((i&0x3)<<3)));
		else
			CAN0_WORD1(txb)=(CAN0_WORD1(txb)&~(0xFF<<(24-((i&0x3)<<3))))|(Frame.Data[i]<<(24-((i&0x3)<<3)));
	}

	CAN0_CS(txb)&=~CAN_CS_DLC_MASK;
	CAN0_CS(txb)|=(Frame.Length<<CAN_CS_DLC_SHIFT)|CAN_CS_CODE(0x0C/*ONCE*/);

	return 1;
}
