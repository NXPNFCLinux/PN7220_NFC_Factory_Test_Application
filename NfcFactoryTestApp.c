#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static char const DEVICE_NODE[] = "/dev/nxpnfc"; 

#if 1
#define PRINT_BUF(x,y,z)  {int loop; printf(x); \
                           for(loop=0;loop<z;loop++) printf("%.2x ", y[loop]); \
                           printf("\n");}
#else
#define PRINT_BUF(x,y,z)
#endif

static char gNfcController_generation = 0;

static int openNfc(int * handle)
{
    *handle = open(DEVICE_NODE, O_RDWR);
    if (*handle < 0) return -1;
	return 0;
}

static void closeNfc(int handle)
{
    close(handle);
}

static void reset(int handle)
{
    ioctl(handle, _IOW(0xE9, 0x01, uint32_t), 0);
    usleep(10 * 1000);
    ioctl(handle, _IOW(0xE9, 0x01, uint32_t), 1);
}

static int send(int handle, char *pBuff, int buffLen)
{
	int ret = write(handle, pBuff, buffLen);
	if(ret <= 0) {
		/* retry to handle standby mode */
		ret = write(handle, pBuff, buffLen);
		if(ret <= 0) return 0;
	}
	PRINT_BUF(">> ", pBuff, ret);
	return ret;
}

static int receive(int handle, char *pBuff, int buffLen)
{
    int numRead;
    struct timeval tv;
    fd_set rfds;
    int ret;

    FD_ZERO(&rfds);
    FD_SET(handle, &rfds);
    tv.tv_sec = 2;
    tv.tv_usec = 1;
	ret = select(handle+1, &rfds, NULL, NULL, &tv);
	if(ret <= 0) return 0;

    ret = read(handle, pBuff, 3);
    if (ret <= 0) return 0;
    numRead = 3;
	if(pBuff[2] + 3 > buffLen) return 0;

    ret = read(handle, &pBuff[3], pBuff[2]);
    if (ret <= 0) return 0;
    numRead += ret;

	PRINT_BUF("<< ", pBuff, numRead);

	return numRead;
}

static int transceive(int handle, char *pTx, int TxLen, char *pRx, int RxLen)
{
	if(send(handle, pTx, TxLen) == 0) return 0;
	return receive(handle, pRx, RxLen);
}

static void Toggle_ModeSwitch_EMVCo(int handle){
	ioctl(handle, _IOW(0xE9, 0x04, uint32_t),1);
}

static void Toggle_ModeSwitch_NFCForum(int handle){
	ioctl(handle, _IOW(0xE9, 0x04, uint32_t),0);
}

static void Standby (int handle)
{
    char NCIEnableStandby[] = {0x2F, 0x00, 0x01, 0x01};
    char Answer[256];

    transceive(handle, NCIEnableStandby, sizeof(NCIEnableStandby), Answer, sizeof(Answer));
    if((Answer[0] != 0x4F) || (Answer[1] != 0x00) || (Answer[3] != 0x00)) {
        printf("Cannot set the NFC Controller in standby mode\n");
        return;
    }

    /* Wait to allow PN71xx entering the standby mode */
    usleep(500 * 1000);

    printf("NFC Controller is now in standby mode - Press enter to stop\n");
    fgets(Answer, sizeof(Answer), stdin);
}

int main()
{
	int nHandle;
    char NCICoreReset[] = {0x20, 0x00, 0x01, 0x01};
    char NCICoreInit2_0[] = {0x20, 0x01, 0x02, 0x00, 0x00};
    char NCIDisableStandby[] = {0x2F, 0x00, 0x01, 0x00};
    char Answer[256];
    int NbBytes = 0;
	
	printf("\n----------------------------\n");
	printf("NFC Factory Test Application\n");
	printf("----------------------------\n");

	if(openNfc(&nHandle) != 0) {
		printf("Cannot connect to NFC controller\n");
		return -1;
	}

	reset(nHandle);
	
	NbBytes = transceive(nHandle, NCICoreReset, sizeof(NCICoreReset), Answer, sizeof(Answer));
	
    /* Catch potential notification */
    usleep(100*1000);
    NbBytes = receive(nHandle,  Answer, sizeof(Answer));
    if((NbBytes == 13) && (Answer[0] == 0x60) && (Answer[1] == 0x00) && (Answer[3] == 0x02))
    {
	NbBytes = transceive(nHandle, NCICoreInit2_0, sizeof(NCICoreInit2_0), Answer, sizeof(Answer));
        if((NbBytes < 23) || (Answer[0] != 0x40) || (Answer[1] != 0x01) || (Answer[3] != 0x00))    {
            printf("Error communicating with NFC Controller\n");
            return -1;
        }

        printf("PN7220 NFC controller detected\n");
        gNfcController_generation = 3;
    }
    else {
            printf("Wrong NFC controller detected\n");
            return -1;
        }
    
	NbBytes = transceive(nHandle, NCIDisableStandby, sizeof(NCIDisableStandby), Answer, sizeof(Answer));
	
    printf("Select the test to run:\n");
    printf("\t 1. Toggle ModeSwitch into EMVCo mode\n");
    printf("\t 2. Toggle ModeSwitch into NFC Forum mode\n");
    printf("\t 3. Enable Standby mode\n");

    printf("Your choice: ");
    scanf("%d", &NbBytes);

    switch(NbBytes) {
        case 1: Toggle_ModeSwitch_EMVCo(nHandle);              	break;
        case 2: Toggle_ModeSwitch_NFCForum(nHandle);        	break;
        case 3: Standby(nHandle);  	    						break;
        default: printf("Wrong choice\n");  					break;
    }

	fgets(Answer, sizeof(Answer), stdin);
	
	reset(nHandle);
	closeNfc(nHandle);
	
	return 0;
}
