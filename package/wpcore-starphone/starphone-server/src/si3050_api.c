#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <math.h>


#include "include/xw_export.h"

unsigned char tel_busy_status = 0; // value 1 means phone is busy 

void set_phone_busy_status(unsigned char busy)
{
    tel_busy_status = busy;
    _DEBUG("phone status: %s",busy ? "Busy now" : "No using");
}

unsigned char get_phone_busy_status(void)
{
    return tel_busy_status;
}

///////////////////////////////////////////////////////////////////
//  PCM INTERFACE
///////////////////////////////////////////////////////////////////

unsigned short buffer[BUFFER_SIZE];


void si3050_generate_sine(int freq, int volume)
{
    int i = 0;
    for (i = 0; i < BUFFER_SIZE / CHANNEL; i++) {
        double x = i * 2 * 3.1415926 / (SAMPLE_RATE / freq);
        
        unsigned short data = (unsigned short) (volume * sin(x) + 32768);
        if (CHANNEL == 1) {
            buffer[i] = data;
        } else {
            buffer[2 * i] = data;
            buffer[2 * i + 1] = data;
        }
    }
}

void si3050_pcm_init_config(struct pcm_config* config)
{
    unsigned int channels = CHANNEL;
    unsigned int rate = SAMPLE_RATE;
    //unsigned int bits = 16;
    unsigned int period_size = 128;
    unsigned int period_count = 2;
    
    config->channels = channels;
    config->rate = rate;
    config->period_size = period_size;
    config->period_count = period_count;
    config->format = PCM_FORMAT_S16_LE;
    config->start_threshold = 0;
    config->stop_threshold = 0;
    config->silence_threshold = 0;   
}

struct pcm* si3050_get_pcm_out(void)
{
    unsigned int card = 0;
    unsigned int device = 0;
    
    struct pcm_config config;
    struct pcm *pcm_out;

    si3050_pcm_init_config(&config);
    
    pcm_out = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm_out || !pcm_is_ready(pcm_out))
    {
        printf("Unable to open PCM device %u (%s)\n", device, pcm_get_error(pcm_out));
        return NULL;
    }
    
    return pcm_out;
}

void si3050_close_pcm_out(SPS_SYSTEM_INFO_T *sps)
{
    close(sps->si3050_pcm_out->fd);
    free(sps->si3050_pcm_out);
}

struct pcm* si3050_get_pcm_in(void)
{
    unsigned int card = 0;
    unsigned int device = 0;
    
    struct pcm_config config;
    struct pcm *pcm_in;

    si3050_pcm_init_config(&config);
    
    pcm_in = pcm_open(card, device, PCM_IN, &config);
   	if (!pcm_in || !pcm_is_ready(pcm_in)) {
        printf("Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm_in));
        return NULL;
    }
    
    return pcm_in;
}
void si3050_close_pcm_in(SPS_SYSTEM_INFO_T *sps)
{
    close(sps->si3050_pcm_in->fd);
    free(sps->si3050_pcm_in);
}

int si3050_play_sine(void)
{
    struct pcm *pcm_out = si3050_get_pcm_out();
       
    si3050_generate_sine(2000, 10000);
    
    while (1) {
        if (!pcm_write(pcm_out, buffer, BUFFER_SIZE)) {
            //printf("sucess in to out: %d\n", BUFFER_SIZE);	
        } else {
            printf("failed\n");
            break;
        }
    }
    
    return 0;
}

void si3050_pcm_loopback(void)
{
    struct pcm *pcm_out = si3050_get_pcm_out();
    struct pcm *pcm_in = si3050_get_pcm_in();
    unsigned char buff[128];
    
    if (!pcm_out || !pcm_in)
        return;
    
    printf("buffer size:%d %d\n", pcm_get_buffer_size(pcm_out), pcm_get_buffer_size(pcm_in));
    
    while(1) {
        int ret = pcm_read(pcm_in, buff, sizeof(buff)); 
        if (!ret) {
                ret = pcm_write(pcm_out, buff, sizeof(buff));
       		if (!ret) {
                        continue;
       			//printf("sucess in to out: %d\n", sizeof(buff));	
       		} else {
       		        printf("pcm_write return value: %d\n",ret);
       			break;
       		}
       	} else {
       	    printf("pcm_read return value: %d\n",ret);
            break;
        }
    }
    
    printf("exit!\n");
}

///////////////////////////////////////////////////////////////////
// SPI CONTROL
///////////////////////////////////////////////////////////////////
void Si3050_Get_VersionInfo(void)
{
        unsigned char sys_ver_val = 0x00;
        char line_device[4][5] = {"UNKN", "3018", "UNKN", "3019"};
        
        _DEBUG("Get si3050 version infomation Start ...");     

         sys_ver_val = gpio_spi_read(SI3050_REG_CHIP_A_REV);    

	if((sys_ver_val>>4) > 3)
	{
		sys_ver_val &=0xF;
		_ERROR("SI3050 dev id error %x\n",sys_ver_val);	
	}

        _DEBUG("Detected SI3050 revision %u and Line-side device is %s \n",  
                        ((sys_ver_val)&0xF), line_device[(sys_ver_val>>4)&0xF]);
    
	/* This is a simple method of verifying if the deivce is alive */
	if(sys_ver_val == 0)
	{
		return ;
	}
        
       return ;
}

int Si3050_Pcm_DriverInit(SPS_SYSTEM_INFO_T *sps)
{
    sps->si3050_pcm_out = si3050_get_pcm_out();
    if (sps->si3050_pcm_out == NULL)
    {
        _ERROR("open si3050 pcm out device faild ...");
        return -1;
    }

    sps->si3050_pcm_in = si3050_get_pcm_in();
    if (sps->si3050_pcm_in == NULL)
    {
        _ERROR("open si3050 pcm in device faild ...");
        return -1;
    }
    
    printf("[buffer size] OUT: %d IN: %d [fd value] OUT: %d IN: %d\n", 
        pcm_get_buffer_size(sps->si3050_pcm_out), 
        pcm_get_buffer_size(sps->si3050_pcm_in),
    	sps->si3050_pcm_out->fd, sps->si3050_pcm_in->fd);    

    return 0;
}

#if 0
void si3050_sw_reset(SPS_SYSTEM_INFO_T *sps)
{
    unsigned char regCfg = 0;

    
    //Enable si3050 PCM interface 
    regCfg = gpio_spi_read(33);
    regCfg |= (0x1 << 3) | (0x1 << 5); // Enable PCM & u-Law
    regCfg &= ~(0x1 << 4);
    gpio_spi_write(33, regCfg);

    //Specific county Seting for Taiwan
    regCfg = gpio_spi_read(16);
    regCfg &= ~((0x1 << 0) & (0x1 << 1) & (0x1 << 4) &  (0x1 << 6)); // OHS RZ RT
    gpio_spi_write(16, regCfg);

    regCfg = gpio_spi_read(26);
    regCfg |= (0x1 << 6) | (0x1 << 7); // DCV[1:0] = 11
    regCfg &= ~((0x1 << 1) & (0x1 << 4) & (0X1 << 5));
    gpio_spi_write(26, regCfg);

    regCfg = gpio_spi_read(30);
    regCfg &= ~((0x1 << 0) & (0x1 << 1) & (0x1 << 2) & (0x1 << 3) & (0x1 << 4));
    gpio_spi_write(30, regCfg);

    regCfg = gpio_spi_read(31);
    regCfg &= ~(0x1 << 3); // OHS2 = 0
    gpio_spi_write(31, regCfg);    

}
#endif

void Si3050_Pin_Reset(void)
{
	    //set_spiclk_pin_low();
		//set_spics_pin_low();
		//set_spisdi_pin_low();
        usleep(1000);
        set_reset_pin_low(); // RESET
        usleep(50*1000);
		//set_spiclk_pin_high();
        usleep(50*1000);
        usleep(50*1000);
        usleep(50*1000);
        usleep(50*1000);
        set_reset_pin_high(); // RESET
        usleep(50*1000);
        usleep(50*1000);
        usleep(50*1000);
        usleep(50*1000);
        usleep(50*1000);
}


void Si3050_Power_Up_Si3019(void)
{
        gpio_spi_write(6, 0x00);
}

/*******************************************************************************
* FUNCTION:     si3050_hw_reset
*
********************************************************************************
* DESCRIPTION:    Power up the device
*
*******************************************************************************/

bool Si3050_Hw_Reset(void)
{
	unsigned char ac_termination_data;
	unsigned char dc_termination_data;
	unsigned char international_control1_data;
	unsigned char daa_control5_data;
	unsigned char loop_cnt = 0;
	unsigned char data;
	unsigned char interrupt_mask;
	//char i,j;


	/* UINT8 intterupt_mask; */
	/* Enable link + power up line side device */
	gpio_spi_write(SI3050_REG_DAA_CONTROL2, 0x00);

	/* Read FTD */
	data=gpio_spi_read(SI3050_REG_LINE_STATUS);//reg12
	while(((!(data & 0x40)) || data==0xff)
		&& (loop_cnt < SI3050_MAX_FTD_RETRY))
	{
		usleep(2*1000);
		loop_cnt++;
		data=gpio_spi_read(SI3050_REG_LINE_STATUS);
	}
        _DEBUG("read line status data = 0x%x",data);
	if(loop_cnt >= SI3050_MAX_FTD_RETRY)
	{
		//j=sprintf(str,"\nFailed to get FTD register sync\n");
		//ser_Write(str,j);
		_ERROR("Try get line status %d times were faild... ",loop_cnt);
		return 0;
	}


	ac_termination_data = 0x00;
	dc_termination_data = 0xc0;
	international_control1_data = 0x00;
	daa_control5_data = 0x20;


	/* Set AC termination */
	gpio_spi_write(SI3050_REG_AC_TERMIATION_CONTROL, ac_termination_data);//reg30
	/* Set DC termination */
	gpio_spi_write(SI3050_REG_DC_TERMINATION_CONTROL, dc_termination_data);//reg26
	/* Set International Control: set ring impedance, ring detection threshold, on-hook speed , enable/disable iir filter */
	gpio_spi_write(SI3050_REG_INTERNATIONAL_CONTROL1, international_control1_data);//reg16 /* OHS IIRE RZ RT */
	/* Set off-hook speed , enable full scale */
	daa_control5_data |= 0x80 | 0x02;//0x80 | 0x02; /* Move the Pole of the built in filter from 5 Hz to 200 Hz this may affect voice quality */
	gpio_spi_write(SI3050_REG_DAA_CONTROL5, daa_control5_data);//reg31 /* OHS IIRE RZ RT */

	/* Enable Ring Validation */
	/* fmin 10Hz */
	/* fmax 83Hz */
	/* ring confirmation count 150ms */
	/* ring time out 640ms */
	/* ring delay 0ms */
	gpio_spi_write(SI3050_REG_RING_VALIDATION_CONTROL1, 0x16);//reg22 //0x56
	gpio_spi_write(SI3050_REG_RING_VALIDATION_CONTROL2, 0x29);//reg23//0x2b
	gpio_spi_write(SI3050_REG_RING_VALIDATION_CONTROL3, 0x99);//reg24

	/* Set interrupt mask */
	/* interrupt_mask = SI3050_RDT_INT | SI3050_ROV_INT | SI3050_FDT_INT | SI3050_BTD_INT | SI3050_DOD_INT |SI3050_LCSO_INT | SI3050_TGD_INT | SI3050_POL_INT; */
	/* si3050_write_reg(SI3050_REG_INTERRUPT_MASK, interrupt_register, tcid); */
	/* Clear intterupt register */
	/* si3050_write_reg(SI3050_REG_INTERRUPT_SOURCE, 0x00, tcid); */

	//si3050_write_reg(43 ,0x03, tcid);
	//si3050_write_reg(44 ,0x07, tcid);

	interrupt_mask = SI3050_RDT_INT | SI3050_ROV_INT | SI3050_FDT_INT | \
                         SI3050_BTD_INT | SI3050_DOD_INT |SI3050_LCSO_INT | SI3050_TGD_INT | SI3050_POL_INT;
	interrupt_mask=0x80;
	gpio_spi_write(SI3050_REG_INTERRUPT_MASK, interrupt_mask);
	/* Clear intterupt register */
	gpio_spi_write(SI3050_REG_INTERRUPT_SOURCE, 0x00);



	/* Set international control registers */
	gpio_spi_write(SI3050_REG_INTERNATIONAL_CONTROL2, 0x00); //reg17 /* RT2 OPE BTE ROV BTD */
	gpio_spi_write(SI3050_REG_INTERNATIONAL_CONTROL3, 0x00); //reg18 /* RFWE */
	gpio_spi_write(SI3050_REG_INTERNATIONAL_CONTROL4, 0x00); //reg19 /* OVL DOD OPE */

	return 1;
}

/*******************************************************************************
* FUNCTION:     pcm_init
*
********************************************************************************
* DESCRIPTION:    Perform hardware initialization of PCM interface.
*
*******************************************************************************/
void Si3050_Pcm_PortInit(unsigned short timeslot)
{
    unsigned short  pcm_offset;
    unsigned char   pcm_mode;


    pcm_offset = (timeslot)*8;
    pcm_mode = SI3050_PCM_ENABLE ;//| SI3050_PCM_TRI ;//| 1<<1;//PHCF

    gpio_spi_write(SI3050_REG_PCM_TX_LOW, (pcm_offset & 0x0ff));
    gpio_spi_write(SI3050_REG_PCM_TX_HIGH, (pcm_offset >> 8) & 0x0003);
    gpio_spi_write(SI3050_REG_PCM_RX_LOW, pcm_offset & 0x0ff);
    gpio_spi_write(SI3050_REG_PCM_RX_HIGH, (pcm_offset >> 8) & 0x0003);
    gpio_spi_write(SI3050_REG_PCM_SPI_MODE_SELECT, pcm_mode);//reg33

}

/*******************************************************************************
* FUNCTION:     si3050_set_hook
*
********************************************************************************
* DESCRIPTION:    Toggle the hook switch
* turn off off-hook bit 
*******************************************************************************/
void Si3050_Set_Hook(bool si3050_off_hook)
{
    unsigned char data = 0;
    if(si3050_off_hook)
    {
        data = 0x1;
    }
    set_phone_busy_status(data);
    gpio_spi_write(SI3050_REG_DAA_CONTROL1, data);
    
    
}

/*******************************************************************************
* FUNCTION:     si3050_hw_gain_control
*
********************************************************************************
* DESCRIPTION:    Modify the DAA gain - note, not used for normal operation.
*
*******************************************************************************/
void Si3050_Hw_Gain_Control(unsigned char gain_value_high, 
							unsigned char gain_value_low, bool f_is_tx_gain)
{
	if(f_is_tx_gain)
	{
		gpio_spi_write(SI3050_REG_TX_GAIN_CONTROL2, gain_value_high);
		gpio_spi_write(SI3050_REG_TX_GAIN_CONTROL3, gain_value_low);
	}
	else
	{
		gpio_spi_write(SI3050_REG_RX_GAIN_CONTROL2, gain_value_high);//r39
		gpio_spi_write(SI3050_REG_RX_GAIN_CONTROL3, gain_value_low);//r41
		/*
		reg 39
		7:5 Reserved Read returns zero.
		4 RGA2 Receive Gain or Attenuation 2.
		0 = Incrementing the RXG2[3:0] bits results in gaining up the receive path.
		1 = Incrementing the RXG2[3:0] bits results in attenuating the receive path.
		3:0 RXG2[3:0] Receive Gain 2.
		Each bit increment represents 1 dB of gain or attenuation, up to a maximum of +12 dB and
		?5 dB respectively.
		For example:
		RGA2 RXG2[3:0] Result
		X 0000 0 dB gain or attenuation is applied to the receive path.
		0 0001 1 dB gain is applied to the receive path.
		0 :
		0 11xx 12 dB gain is applied to the receive path.
		1 0001 1 dB attenuation is applied to the receive path.
		1 :
		1 1111 15 dB attenuation is applied to the receive path.
		*/

		/*
		reg 41
				7:5 Reserved Read returns zero.
				4 RGA3 Receive Gain or Attenuation 2.
				0 = Incrementing the RXG3[3:0] bits results in gaining up the receive path.
				1 = Incrementing the RXG3[3:0] bits results in attenuating the receive path.
				3:0 RXG3[3:0] Receive Gain 3.
				Each bit increment represents 0.1 dB of gain or attenuation, up to a maximum of 1.5 dB.
				For example:
				RGA3 RXG3[3:0] Result
				X 0000 0 dB gain or attenuation is applied to the receive path.
				0 0001 0.1 dB gain is applied to the receive path.
				0 :
				0 1111 1.5 dB gain is applied to the receive path.
				1 0001 0.1 dB attenuation is applied to the receive path.
				1 :
				1 1111 1.5 dB attenuation is applied to the receive path.
		*/
	}
}


/*******************************************************************************
* FUNCTION:     si3050_set_lowpwr_path
*
********************************************************************************
* DESCRIPTION:  set data path on onhook line monitor state to transfer CID
*
*******************************************************************************/
void Si3050_Set_Lowpwr_Path(void)
{
    gpio_spi_write(SI3050_REG_DAA_CONTROL1, 0x08);
}

/*******************************************************************************
* FUNCTION:     si3050_clear_lowpwr_path
*
********************************************************************************
* DESCRIPTION:  disable low power data path on onhook state
*
*******************************************************************************/
void Si3050_Clear_Lowpwr_Path(void)
{
    gpio_spi_write(SI3050_REG_DAA_CONTROL1, 0x00);
}


void Si3050_Dial_PhoneNum(char dial_num)
{
	int retVal = 0;
        int pcm_buf_size = 0;
        char wav_file[32] = {'\0'};
	char *pcm_rw_buf = NULL;
	char *dtmf_sound = "0123456789abcdefABCDEF#*";
        
	WAV_T *wav = NULL;


	SPS_SYSTEM_INFO_T *DTSystemInfo = XW_Global_InitSystemInfo();
	if(DTSystemInfo->si3050_pcm_out == NULL)
	{
		_ERROR("DTSystemInfo->si3050_pcm_out is NULL");
		return ;
	}

        if((strchr(dtmf_sound, dial_num) -dtmf_sound) > strlen(dtmf_sound))
        	return ;

        _DEBUG("Dial Number :  %c",dial_num);

        //config Si3050
        Si3050_Set_Hook(HI_TRUE);

	// Create wav sound file path
        sprintf(wav_file, "/root/starphone/res/%c.wav", dial_num);
	//if(access(wav_file, 0) < 0)
	//{
	//	_ERROR("dtmf sound file not found: %s",wav_file);
	//}
        _DEBUG("found wav file: %s",wav_file);

        // Load wav sound file and return the pcm data to buffer
        //load_wave(wav_file, &wav_fmt, pcm_rw_buf, &pcm_buf_size);
        //_DEBUG("[wav file info] FormatTag: %s Channel: %d SamplePerSec:%ld",
        //        wav_fmt.wFormatTag, wav_fmt.wChannels, wav_fmt.dwSamplesPerSec);
        //_DEBUG("[wav file info] BitPerSample: %ld buf_addr: %p buf_size: %ld",
        //        wav_fmt.wBitsPerSample, pcm_rw_buf, pcm_buf_size);        

	wav = wav_open(wav_file);
	if(NULL == wav)
	{
		_ERROR("open wav file %s faild...", wav_file);
		wav_close(&wav);
	}
	wav_dump(wav);

	wav_rewind(wav);

	pcm_buf_size = wav->file_size;
	pcm_rw_buf = (char *)malloc(wav->file_size);
	if(pcm_rw_buf == NULL)
	{
		_ERROR("malloc pcm data buffer faild..");
		wav_close(&wav);
		return ;
	}	
	_DEBUG("pcm data buf addr: 0x%x size: %d Byte",pcm_rw_buf, pcm_buf_size);
	
	// transfer dial number .wav data to Si3050 pcm port
	_DEBUG("[pcm fd] OUT_fd: %d",DTSystemInfo->si3050_pcm_out->fd);
	retVal = pcm_write(DTSystemInfo->si3050_pcm_out, pcm_rw_buf, pcm_buf_size);
	if(retVal < 0)
	{
		_ERROR("pcm_write return value(ErrNo:%d) error !",retVal);
	}

	wav_close(&wav);

        usleep(50*1000);
        usleep(50*1000);

        return ;
}


/*------------------------------------------------------------------------------*/
/* FUNCTION:    XW_Si3050_DAA_System_Init                                                  */
/*------------------------------------------------------------------------------*/
/* DESCRIPTION:    Perform hardware initialization of TID analog interface.     */
/*------------------------------------------------------------------------------*/
int XW_Si3050_DAA_System_Init(void)
{
        //unsigned char regCfg = 0;

        SPS_SYSTEM_INFO_T *DTSystemInfo = XW_Global_InitSystemInfo();

        gpio_spi_port_dir_init();

        // reset si3050 with set low level for reset pin
        Si3050_Pin_Reset();
        _DEBUG("Reset si3050 hardware complete...");
    
        // Check the Version to make sure SPI Conmunication is OK
        Si3050_Get_VersionInfo();
        
	/* Initialize and power up DAA */
	if(Si3050_Hw_Reset() == 0)
	{
            _ERROR("si3050 config reset register value faild...");
	    return -1;
	}	
        _DEBUG("Reset si3050 softare complete...");
        
        
	/* enable PCM and assign timeslot for PCM */
	Si3050_Pcm_PortInit(1);
        if(Si3050_Pcm_DriverInit(DTSystemInfo) < 0)
        {		
            _ERROR("si3050 pcm driver devices init faild...");
	    return -1;
	}

	/* Enable intterupt */
	gpio_spi_write(SI3050_REG_CONTROL2, 0x83); //0x87
 
        //Si3050_Power_Up_Si3019();

        return 0;
}

void *XW_Pthread_ModemCtrlDeamon(void *args)
{
	char *sock_send_msg = NULL;
        thread_arg *pthread_client = NULL; 
    
        MspSendCmd_t cmdData;	//ÏûÏ¢¶ÓÁÐ´«Êä½á¹¹	
	STATE_PREVIEW *p;
        PTHREAD_BUF send_buf;
        SPS_SYSTEM_INFO_T *DTSystemInfo = XW_Global_InitSystemInfo();
        
        p = (STATE_PREVIEW *)XW_ManagePthread_GetPthreadState(PTHREAD_MODEM_CTRL_ID, 0);
        p->power = PTHREAD_POWER_ON;
        p->state = ALIVE;
        
        while(p->power == PTHREAD_POWER_ON)
        {
                memset(&send_buf, 0, sizeof(PTHREAD_BUF));
                XW_ManagePthread_ReadSignal(&send_buf, PTHREAD_MODEM_CTRL_ID, HI_TRUE);                
                if(send_buf.start_id != PTHREAD_CLIENT_MANAGE_ID || strlen(send_buf.m_buffer) == 0)
                {
                        if (send_buf.start_id == PTHREAD_MAIN_ID && send_buf.m_signal == EXIT)
                        {      
                                p->state = EXIT;
                                break;
                        }    
                        _DEBUG("start_id = %d, strlen(send_buf.m_buffer) = %d",
                                            send_buf.start_id, strlen(send_buf.m_buffer));
                        sleep(2);
                        continue;
                }

                //TODO:
                pthread_client = (thread_arg *)(send_buf.m_args);
                if(pthread_client == NULL)
                {
                        _ERROR("client[%d] address transfer faild...",send_buf.m_value);
                }
                else
                {
                        _DEBUG("[Client] IP=%s,  Msg=%s", 
                                         inet_ntoa(pthread_client->caddr.sin_addr), send_buf.m_buffer);
                }
                
                //Paser data which is transfer from phone
                if(strncmp(send_buf.m_buffer, "help", 4)==0) 
                {
        		sock_send_msg = "This is help message\n";
        		//_send(send_buf.m_args->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
                else if(strncmp(send_buf.m_buffer, "hangup", 6)==0) 
                {        		
        		//Modem_Hangup();
                        Si3050_Set_Hook(0);
        		sock_send_msg ="ok\n";
         		set_phone_busy_status(0);
        		sock_send(pthread_client->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
                else if(strncmp(send_buf.m_buffer, "dial:", 5)==0)// dial:12345
                { 
        		if(get_phone_busy_status() == 1)
                        {
        			sock_send_msg = "busy\n"; //  ééæ¥èçæ?
        			sock_send(pthread_client->connfd, sock_send_msg, strlen(sock_send_msg));
                                _DEBUG("phone is busy now !");
        			continue;
        		}
                        set_phone_busy_status(1);

                        //Parse dial number
                        unsigned char i = 0;
                        char *dial_str =&send_buf.m_buffer[5];
                        for(i = 0; i < (strlen(send_buf.m_buffer)-6); i++)
                                Si3050_Dial_PhoneNum(dial_str[i]);
                
                        // if off-hook, then recv & send sound data to socket
                                 
        	}
        	else if(strncmp(send_buf.m_buffer, "list", 4)==0)
                {
        		char buf2[MAX+1][100];
        		strcpy(buf2[0],  "Client list:\n");
        		//j=1;
        		//for(i=0;i<MAX;i++) {
        		//	if(send_buf.m_args->connfd != 0) {
        		//		sprintf(buf2[j], "%d: %s:%d\n", j, 
        		//				inet_ntoa(clients[i]->caddr.sin_addr), ntohs(clients[i]->caddr.sin_port));
        		//		j++;
        		//	}
        		//}
        		//len = 0;
        		//for(i=0;i<j;i++) {
        		//	len += strlen(buf2[i]);
        		//}
        		//sock_send_msg = calloc(1, len); // init to 0
        		//for(i=0;i<j;i++) {
        		//	strcat(sock_send_msg, buf2[i]);
        		//}
        		//_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
                /*        	
        	else if(strncmp(send_buf.m_buffer, "key:", 4)==0) { // éè©±ä¸­çæéµ:0~9, *, #
        		if(busy==0) {
        			sock_send_msg = "no communication\n"; // éå¨ææ©çæ? ä¸è½ç¨éåæä»?
        			_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        			return;
        		}
        		modem_mute = 1; // éé³, é¿åå¹²æ¾ dtmf tone
        		usleep(500000); // delay, å çº modem åºè²é³æ¬ä¾å°±æå»¶é?
        		//char number[50];
        		//strncpy(number, buf+5, strlen(buf)-5);
        		char buf2[3] = {0x21, 0x1, 0};
        		if(send_buf.m_buffer[4]>=49 && send_buf.m_buffer[4]<=57) { // 1~9ç´æ¥é?
        			buf2[2] = send_buf.m_buffer[4]-48;
        		}else if(send_buf.m_buffer[4]=='*')  {
        			buf2[2] = 0xb;
        		}else if(send_buf.m_buffer[4]=='#') {
        			buf2[2] = 0xc;
        		}else if(send_buf.m_buffer[4]=='0') {
        			buf2[2] = 0xa;
        		}
        		//send_pstn(3, buf2);
        		sock_send_msg = "key_ok\n";
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        		usleep(500000); // delay, å çº modem ?ºè²é³æ¬ä¾å°±æå»¶é²
        		modem_mute = 0;
        		//exit(-1);
        	}        	
        	else if(strncmp(send_buf.m_buffer, "test_ring:", 10)==0) { // test_ring:12345 æ¸¬è©¦ä¾é»
        		sock_send_msg ="ok\n";
        		char buf2[100];
        		sprintf(buf2, "external:%s\n", send_buf.m_buffer+10);
        		//broadcast_clients(buf2);
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strcmp(send_buf.m_buffer, "ring_end")==0) { // é¿é´åæ­¢: ä¾é»åæ­¢, æä¾é»å·²è¢«æ¥èµ·éç¥å¶ä»äººéédialog
        			sock_send_msg ="ok\n";
        			broadcast_clients("ring_end\n");
        			_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strcmp(send_buf.m_buffer, "internal_end")==0) { // çµæå§ç·éè©±
        		sock_send_msg = "internal_end\n";
        		if(arg->peer != NULL) {
        			_send(arg->peer->connfd, sock_send_msg, strlen(sock_send_msg));
        			(*arg->peer).peer = NULL; // æ¸é¤å°æ¹
        			arg->peer = NULL; // æ¸é¤èªå·±ç´é?
        			arg->busy=0;
        			printf("id=%d",arg->id);
        		} else {
        			printf("peer is null??\n");
        		}
        		sock_send_msg ="ok\n";
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strcmp(send_buf.m_buffer, "pick_up")==0) { // å¤ç·æäººæ¥èµ·äº?
        		//broadcast_clients("ring_end\n");
        		//thread_arg_hook arg_hook;
        		//arg_hook.caddr = arg->caddr;
        		//arg_hook.number = NULL;
        		pthread_t id;
        		//_pthread_create(&id, (void*)off_hook, &arg_hook);
        		//pthread_join(id, NULL);
        		sock_send_msg = "on_hook\n";
        		//_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strncmp(send_buf.m_buffer, "internal:", 9)==0) { // test_dial:12345 å§ç·å¼å«
        		// find client with the id
        		for(i=0;i<MAX;i++) {
        			printf("enter internal top\n,internal id =%d\n",clients[i]->id);
        			if(clients[i]->id == atoi(send_buf.m_buffer+9)) { // atoi æèªåå¿½ç¥ç¡æ³è½çå­å?
        				// å°æ¹å¿ç·
        				if(clients[i]->peer != NULL || clients[i]->busy==1) {
        					sock_send_msg ="busy\n";
        					_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        					return;
        				}
        				char buf2[32];
        				// åå³å°æ¹ ip
        				sprintf(buf2, "internal_ip:%s\n", inet_ntoa(clients[i]->caddr.sin_addr));
        				_send(arg->connfd, buf2, strlen(buf2));
        				arg->peer = clients[i]; // ç´ééè©±å°è±¡
        				
        				// éç¥å°æ¹æäººæ¾ä»
        				sprintf(buf2, "internal:%d,%s\n", 
        						arg->id, 
        						inet_ntoa(arg->caddr.sin_addr));
        				_send(clients[i]->connfd, buf2, strlen(buf2));
        				clients[i]->peer = arg;
        				
        				return;
        			}
        		}
        		sock_send_msg ="not found\n";
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strncmp(send_buf.m_buffer, "register:", 9)==0) { // test_dial:12345 è¨»ååæ©èç¢¼
        		// check exist
        		for(i=0;i<MAX;i++) {
        			if(clients[i]->id == atoi(send_buf.m_buffer+9)) { // atoi æèªåå¿½ç¥ç¡æ³è½çå­å?
        				char *buf2 = "register_exist\n";
        				_send(arg->connfd, buf2, strlen(buf2));
        				return;
        			}
        		}
        		// æ²éè¤? ç»è¨æå
        		arg->id = atoi(send_buf.m_buffer+9);
        		sock_send_msg ="register_ok\n";
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strncmp(send_buf.m_buffer, "deny", 4)==0) { // ææ¥å¤ç·
        		// æ¿èµ·åé¦¬ä¸ææ?
        		char buf2[2] = {0x12, 0}; // off-hook
        		send_pstn(2, buf2);
        		
        		buf2[0] = 0x13; // on-hook
        		send_pstn(2, buf2);
        		
        		sock_send_msg ="ok\n";
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
        	else if(strncmp(send_buf.m_buffer, "switch", 6)==0) { // ææ¥
        		if(busy==1) {
        			//pstn_switch(); // ææåé¦¬ä¸æ¿èµ·ä¾
        			
        			sock_send_msg = "switch_ok\n";
        			_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        			busy = 1; // FIXME: å¼·å¶åæå®æ1, è¦æ¾æ¯é£éæä»è®æ?0 ç?
        		} else {
        			sock_send_msg = "no communication\n"; // éå¨ææ©çæ? ä¸è½ç¨éåæä»?
        			_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        		}
        	}
        	else {
        		sock_send_msg = "unknown command\n";
        		_send(arg->connfd, sock_send_msg, strlen(sock_send_msg));
        	}
                */
                //sleep(10);
        }

        p->state = EXIT;
        PTHREAD_BUF  signal;
        if(XW_ManagePthread_SendSignal(&signal, PTHREAD_MODEM_CTRL_ID) == false)
        {
                _ERROR("PTHREAD_MODEM_CTRL_ID[%d] error !'\n", PTHREAD_MODEM_CTRL_ID);
        }
        
	_DEBUG("modem control thread exit !");

        return 0;
} 

