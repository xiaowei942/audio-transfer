/* Copyright (C) 2008 The Android Open Source Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <string.h> 

#include <jni.h>
#include <android/log.h>
#include <pthread.h>

#define SECONDS 10
#define INPUT_DEVNAME "/dev/msm_pcm_in"
#define OUTPUT_DEVNAME "/dev/msm_pcm_out"

//#define __DEBUG
#define LOG_TAG "WEI:jni "

#ifdef __DEBUG
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) 
#define LOGE(...) 
#define LOGD(...)
#endif
#define AUDIO_IOCTL_MAGIC 'a'

#define AUDIO_START        _IOW(AUDIO_IOCTL_MAGIC, 0, unsigned)
#define AUDIO_STOP         _IOW(AUDIO_IOCTL_MAGIC, 1, unsigned)
#define AUDIO_FLUSH        _IOW(AUDIO_IOCTL_MAGIC, 2, unsigned)
#define AUDIO_GET_CONFIG   _IOR(AUDIO_IOCTL_MAGIC, 3, unsigned)
#define AUDIO_SET_CONFIG   _IOW(AUDIO_IOCTL_MAGIC, 4, unsigned)
#define AUDIO_GET_STATS    _IOR(AUDIO_IOCTL_MAGIC, 5, unsigned)

#define GET_ARRAY_LEN(array,len) {len = (sizeof(array) / sizeof(array[0]));}

/* For big-endin purpose */
#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
	uint32_t riff_id;
	uint32_t riff_sz;
	uint32_t riff_fmt;
	uint32_t fmt_id;
	uint32_t fmt_sz;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;       /* sample_rate * num_channels * bps / 8 */
	uint16_t block_align;     /* num_channels * bps / 8 */
	uint16_t bits_per_sample;
	uint32_t data_id;
	uint32_t data_sz;
};

struct msm_audio_config {
    uint32_t buffer_size;
    uint32_t buffer_count;
    uint32_t channel_count;
    uint32_t sample_rate;
    uint32_t codec_type;
    uint32_t unused[3];
};

struct msm_audio_stats {
    uint32_t out_bytes;
    uint32_t unused[3];
};

#define INPUT_FLAG (1<<0) 
#define OUTPUT_FLAG (1<<1)

struct audio_struct {
	int fd_input;
	int fd_output;

	unsigned char *msg;
	char *outbuf;

	unsigned play_rate;
	unsigned play_channels;
	unsigned rec_rate;
	unsigned rec_channels;
	unsigned flags;

	struct msm_audio_config input_config;
	struct msm_audio_config output_config;
	struct msm_audio_stats audio_stats;
} my_audio_struct;

char bits2[] = {
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,

	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f
};

//扩展位0用数据信息
char bit0[] = {
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80
};

//扩展位1用数据信息
char bit1[] = {
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f
};

//输出消息头
char head_out[] = {
	0xaa,0xaa,0xaa,0xaa,
	0xaa,0xaa,0xaa,0xaa,
	0xaa,0xaa,0xff,0xff
};

//输出消息尾
char tail_out[] = {
	0x00,0x00
};

//输入消息头
char head_in[] = {
	0xaa,0xaa,0xaa,0xaa,
	0xaa,0xaa,0xaa,0xaa,
	0xaa,0xaa,0xff,0xff,
	0xff,0xff
};

//输入消息尾
char tail_in[] = {
	0xff,0xff,0xff,0xff
};

#define SECONDS 10 //10秒
#define DATA_LEN_1S 44100*2 	//单声道，44100采样，16位精度
#define DATA_LEN DATA_LEN_1S*SECONDS
#define BUF_LEN DATA_LEN/2

#define HIGH_GATE 32767-8192 //高电平数据阈值
#define LOW_GATE -32767+8192 //低电平数据阈值

char input[DATA_LEN]; // 实际输入数据
char output[480000]; // 实际输出数据
unsigned out_index = 0; //实际输出索引
unsigned out_count = 0; //实际输出计数
int current_pos = 0; // 用来解析数据的当前数据位置索引

int onetime=0;  //确保预填充一次
char *prefill; 	//预填充缓冲区
int head_len=0; //消息头长度
int tail_len=0; //消息尾长度
int msg_len=0; 	//消息长度
int sum=0; 	//消息字节总数


char temp[1024]; //用来存放接收的数据
char buf_out[1024]; //用来存放解析到的有效数据


/********************************************************
 * 函数：	open_files    				*
 * 参数： 	
 * 		flags 打开的文件标识 		        *
 * 	 	    可选值：  			        *
 * 			INPUT_FLAG  打开输入文件        *
 * 			OUTPUT_FLAG 打开输出文件        *
 * 返回值： 	返回成功打开的文件标识，打开失败返回0。 *
 * 说明： 	打开输入输出音频的设备文件无 					*
 * 							*
 ********************************************************/
int open_files(unsigned flags)
{
	int ret=0;
	int fd_in, fd_out;
	LOGI("open_file --> flags: %d",flags);
	if(flags & INPUT_FLAG)
	{
		fd_in = open(INPUT_DEVNAME,O_RDWR);
		if(fd_in < 0)
		{
			LOGE("Cannot open input file");		
		}
		else
		{
			ret |= INPUT_FLAG;
		}
		LOGI("Open input success");
		my_audio_struct.fd_input = fd_in;
	}

	if(flags & OUTPUT_FLAG)
	{
		fd_out = open(OUTPUT_DEVNAME,O_RDWR); 
		if(fd_out < 0)
		{
			LOGE("Cannot open output file");		
		}
		else
		{
			ret |= OUTPUT_FLAG;
		}
		LOGI("Open output success");
		my_audio_struct.fd_output = fd_out;
	}

	return ret;
}



/****************************************************************
 * 函数：	set_params    					*
 * 参数：  							*
 * 		flags 打开的文件标识 		      		*
 * 	 	    可选值：  			       		*
 * 			INPUT_FLAG  打开输入文件        	*
 * 			OUTPUT_FLAG 打开输出文件        	*
 * 		play_rate 播放的采集频率，暂时仅为44100		*
 * 		play_rate 播放的采集频率，暂时仅为44100		*
 * 返回值： 	返回成功打开的文件标识，打开失败返回0。 	*
 * 说明： 	打开输入输出音频的设备文件无 			*
 * 								*
 ****************************************************************/
int set_params(unsigned play_rate, unsigned play_channels, unsigned rec_rate, unsigned rec_channels, unsigned flags)
{
	if(flags & INPUT_FLAG)
	{
		/* config change should be a read-modify-write operation */
		if (ioctl(my_audio_struct.fd_input, AUDIO_GET_CONFIG, &my_audio_struct.input_config)) {
			LOGE("cannot get input config");
			return -1;
		}

		my_audio_struct.input_config.channel_count = 1;
		my_audio_struct.input_config.sample_rate = 44100;
		if (ioctl(my_audio_struct.fd_input, AUDIO_SET_CONFIG, &my_audio_struct.input_config)) {
			LOGE("cannot write audio config");
			return -1;
		}

		if (ioctl(my_audio_struct.fd_input, AUDIO_GET_CONFIG, &my_audio_struct.input_config)) {
			LOGE("cannot read audio config");
			return -1;
		}

		int sz = my_audio_struct.input_config.buffer_size;

		LOGE("buffer size %d x %d\n", sz, my_audio_struct.input_config.buffer_count);
		if (sz > sizeof(input)) {
			LOGE("buffer size %d too large\n", sz);
			return -1;
 		}
		
	}

	if(flags & OUTPUT_FLAG)
	{
		if(ioctl(my_audio_struct.fd_output, AUDIO_GET_CONFIG, &my_audio_struct.output_config))
		{
			LOGE("Cannot get output config");
			return -1;
		}

		my_audio_struct.output_config.channel_count = play_channels;
		my_audio_struct.output_config.sample_rate = play_rate;
		
		LOGI("play_channels: %d",play_channels);
		LOGI("play_rate: %d",rec_rate);

		if(ioctl(my_audio_struct.fd_output, AUDIO_SET_CONFIG, &my_audio_struct.output_config))
		{
			LOGE("Cannot set output config");
			return -1;
		}
	
		ioctl(my_audio_struct.fd_output, AUDIO_START, 0);

	}
	LOGI("Set params success\n");
	return 0;
}

int unit_init(unsigned play_rate, unsigned play_channels, unsigned rec_rate, unsigned rec_channels, unsigned flags)
{
	if(open_files(flags) == 0)
		return -1;

	if(set_params(play_rate, play_channels, rec_rate, rec_channels, flags) != 0)
		return -1;

	LOGI("Init success\n");
	return 1;
}

void unit_destroy()
{
	if(!prefill)
		free(prefill);

	if(my_audio_struct.fd_output)
		ioctl(my_audio_struct.fd_output, AUDIO_STOP, 0);

	if(my_audio_struct.fd_output)
		close(my_audio_struct.fd_output);
	
	if(my_audio_struct.fd_input)
		close(my_audio_struct.fd_input);
}

int make_databit_(int bit)
{
	
}

int test_doSend()
{
	char *buf;
	char *buff;
#if 1
	int fd = open("/data/out.wav",O_RDONLY);
	if(fd<0)
	{
		LOGE("Cannot open out.wav");
	}
	LOGI("Open out.wav success");
#endif	
	int number = 44100*2*2*5;
	buf = malloc(number);

	memset(buf, 0x00, number);

#if 1
	//lseek(fd, 44, SEEK_SET);

	//if(read(fd, buf, number) != number)
	{
	//	LOGE("Cannot read media data");
	}
	
	int m,n;
	int len=200;

LOGI("1");

	for(m=0; m<len; m++){
	//	LOGI("1-m: %d", m);
		for(n=0; n<72; n++){
			//LOGI("1-n: %d", n);
			buf[n] = bits2[n];
		}
		buf += 72;
	}
LOGI("11");
	buf -= len*72;

	for(m=0; m<len; m++){
	//	LOGI("11111-m: %d", m);
		for(n=0; n<72; n++){
			//LOGI("111111-n: %d", buf[n]);
		}
		buf += 72;
	}
	
	buf -= len*72;
	//memset(buf,0xaa,44100*2*5);
#endif

	for(n=0; n<my_audio_struct.output_config.buffer_count; n++)
	{
		if(write(my_audio_struct.fd_output, buf, 4800) != 4800)
		{
			LOGI("2");
			break;
		}
	}

	ioctl(my_audio_struct.fd_output, AUDIO_START, 0);
	LOGI("3");
	for(;;)
	{
		LOGI("Now playing");
#if 0
		int ret = read(fd, buf,4800);
		if(ret != 4800)
		{
			LOGE("Read data faild");
		}

		write(my_audio_struct.fd_output, buf, 4800);
#endif
		//for(n=0; n<4800;n++)
			//LOGI("BUF: %d",buf[n]);
		if(write(my_audio_struct.fd_output, buf, 4800) != 4800)
		{
			
			LOGI("Write exit");
			break;
		}
		buf += 4800;
	}
//	ioctl(my_audio_struct.fd_output, AUDIO_STOP, 0);
}


int test_do_command()
{
	char *buf,*temp;

	temp = output;

	int n=0;
	int number = my_audio_struct.play_rate*my_audio_struct.play_channels*2*5;

	buf = malloc(number);
	memset(buf, temp, number);

	for(n=0; n<my_audio_struct.output_config.buffer_count; n++)
	{
		if(write(my_audio_struct.fd_output, temp, my_audio_struct.output_config.buffer_size) != my_audio_struct.output_config.buffer_size)
		{
			LOGI("2");
			break;
		}
	}

	
	ioctl(my_audio_struct.fd_output, AUDIO_START, 0);

	for(;;)
	{
		LOGI("Now playing");
		if(write(my_audio_struct.fd_output, temp, my_audio_struct.output_config.buffer_size) != my_audio_struct.output_config.buffer_size)
		{
			
			LOGI("Write exit");
			break;
		}
		temp += my_audio_struct.output_config.buffer_size;
	}
}

byte2chars(char in)
{
	int i,j;
	//依次转换输出数据字符数组中的每一位(由高到低)
	for(i=7;i>=0;i--)
	{
		if((in>>i)&0x1)
		{
			for(j=0;j<36;j++)
			{
				output[out_index] = bit0[j];
				out_index++;
			}
		}
		else
		{
			for(j=0;j<36;j++)
			{
				output[out_index] = bit1[j];
				out_index++;
			}
		}
	}
}

// 字节扩展为表示位信息的字符数组，带起始、停止位
byte2chars_bits(char in)
{
	int i,j;

	//开始位
	for(j=0;j<36;j++)
	{
		output[out_index] = bit1[j];
		out_index++;
	}

	//依次转换输出数据字符数组中的每一位(由高到低)
	for(i=7;i>=0;i--)
	{
		if((in>>i)&0x1)
		{
			for(j=0;j<36;j++)
			{
				output[out_index] = bit0[j];
				out_index++;
			}
		}
		else
		{
			for(j=0;j<36;j++)
			{
				output[out_index] = bit1[j];
				out_index++;
			}
		}
	}

	//停止位
	for(j=0;j<36;j++)
	{
		output[out_index] = bit0[j];
		out_index++;
	}
}

int test_transferOneFrame()
{
	int i;
	char command[] = {0xaa,0xab,0xaf,0xff,0x00,0xff,0x00,0x00};
	for(i=0;i<8;i++)
	{
		LOGI("command[%d]: 0x%x",i,command[i]);
		byte2chars(command[i]);
	}
	test_do_command();
}

int make_message(char *dest, unsigned char *msg, int msg_len)
{
	LOGI("make_message");

	memcpy(dest, msg, msg_len);

#if 1	
	LOGI("msg_len:%d",msg_len);
	
	int t;
	for(t=0;t<sum;t++)
	{
		LOGI("dest[%d]: 0x%x",t,dest[t]);
	}
#endif
	return sum;
}

int do_send(struct audio_struct *aud)
{
	LOGI("do_send");
	if(!aud)
		return -EINVAL;

	char *temp = output;
	if(!onetime)
	{
		int n=0;
		for(n=0; n<aud->output_config.buffer_count; n++)
		{
			if(write(aud->fd_output, prefill, aud->output_config.buffer_size) != aud->output_config.buffer_size)
			{
				LOGI("Prefill failed");
				return -1;
			}
		}
		onetime = 1;
		ioctl(aud->fd_output, AUDIO_START, 0);
	}
	int i;

	for(i=0; i<out_count/aud->output_config.buffer_size; i++)
	{
		LOGI("Sending ...");
		int count;
		if(write(aud->fd_output, temp, aud->output_config.buffer_size) != aud->output_config.buffer_size)
		{
			LOGI("Send failed");
			return -1;
		}
		temp += aud->output_config.buffer_size;
		count += aud->output_config.buffer_size;
	}
	LOGI("Send success");
//	ioctl(aud->fd_output, AUDIO_STOP, 0);
	return 0;
}

void transform_message(struct audio_struct *aud)
{
	int i,j;

	//以输出缓冲区大小对齐数据（向上），并填充为‘0’
	out_count = ((out_index/my_audio_struct.output_config.buffer_size)+1)*my_audio_struct.output_config.buffer_size;
	for(i=out_index; i<out_count; i++)
	{
		for(j=0;j<36;j++)
		{
			output[i] = bit1[j];
		}
	}

	LOGI("out_count: %d", out_count);
	LOGI("out_index: %d",out_index);

}

int send_message(struct audio_struct *aud, const int count)
{
	
	int i,j;
	LOGI("count = %d",count);

	out_index=0;

	for(i=0;i<head_len;i++)
	{
		//LOGI("head[%d]: 0x%x", i, head_out[i]);
		byte2chars(head_out[i]);
	}

	for(i=0;i<count;i++)
	{
		//LOGI("msg[%d]: 0x%x", i, aud->msg[i]);
		byte2chars_bits(aud->msg[i]);
	}
/*
	unsigned char sum=0;

	for(i=0;i<count;i++)
	{
		sum += aud->msg[i];
	}

	//LOGE("~sum: 0x%x", ~sum&0xff);
	byte2chars_bits(~sum&0xff);
*/
/*
	for(i=0;i<count;i++)
	{
		byte2chars_bits(~aud->msg[i]);
		LOGI("~msg[%d]: 0x%x", i, ~aud->msg[i]);
	}
*/
	for(i=0;i<tail_len;i++)
	{
		//LOGI("tail[%d]: 0x%x", i, tail_out[i]);
		byte2chars(tail_out[i]);
	}

	
	transform_message(aud);

	//设置预填充缓冲区为输出’0‘
	
	int prefill_size = my_audio_struct.output_config.buffer_count*my_audio_struct.output_config.buffer_size;

	if(!prefill)
	{
		LOGI("Malloc prefill");
		prefill = malloc(prefill_size);

		for(i=0; i<prefill_size; i++)
		{
			for(j=0;j<36;j++)
			{
				prefill[i] = bit1[j];
			}
		}
	}

	int ret = do_send(aud);
	return ret;
}

int read_byte(char *temp, int end)
{
	LOGI("read_byte --> current_pos: %d", current_pos);
	unsigned char tmp=0;
	//current_pos+=18;
	int i,j=current_pos,pos=current_pos;
	short *buf = input; 

	if(buf[j] >= LOW_GATE)
	{
		LOGI("    -->not a valid start bit");
		return 0; 	//Bad start bit
	}

	while(j<end)
	{
		// 连续3个高电平，则第一个高电平为数据位起始位置
		if(buf[j] >= HIGH_GATE && buf[j-1] >= HIGH_GATE && buf[j-2] >= HIGH_GATE)
		{
			current_pos = j-2; //将当前位置设置为高电平起始位置
			break;
		}
		
		// 数据起始标志后，如果18个数据还没到数据位起始位置，则出错
		if((j-pos)>22)
		{
			LOGI("    -->no data start bit - %d", j);
			return 0;
		}

		j++;
	}
	//LOGI("    -->current_pos: %d", current_pos);
	//调整到第一位的中间位（0b110表示位1，0b100表示位0）
	for(i=7,j+=9+18; i>=0; i--,j+=3*18)
	{
	//	LOGI("    current_pos-read: %d", j);
		if(buf[j] > HIGH_GATE)
		{
			tmp |= (1<<i);
		}
		else if(buf[j] < LOW_GATE)
		{
			tmp &= ~(1<<i);
		}
		else
		{
			LOGE("INVAL");
		}
	}

	LOGI("    -->current_pos-after-read: %d", current_pos);
	//将当前位置设置为停止位上
	current_pos = j-18+2;

	LOGI("    -->current_pos-stopbit: %d", current_pos);
	// 错误停止位
	if(buf[current_pos] <= HIGH_GATE)
	{
		LOGE("    --> Error on stop bit: %d", current_pos-18);
		return 0;
	}

	*temp = tmp;
	pos=current_pos;
	LOGI("    tmp: %d", *temp);

	while(j<end)
	{
		// 连续3个高电平，则第一个高电平为数据位起始位置
		if(buf[j] <= LOW_GATE && buf[j-1] <= LOW_GATE && buf[j-2] <= LOW_GATE)
		{
			current_pos = j-2; //将当前位置设置为高电平起始位置
			break;
		}
		
		// 数据起始标志后，如果18个数据还没到数据位起始位置，则出错
		if((j-pos)>22)
		{
			if(is_one_frame_end(end))
				return 1;
			LOGI("    -->no start bit - %d", j);
			return 0;
		}

		j++;
	}

	return 1;
}

int is_one_frame_end(int end)
{
	LOGI("is_one_frame_end --> current_pos: %d" ,current_pos);

	int i=current_pos,j=0,k=end;
	signed short *buf = input;
	if((end-i)<18*12)
		return 0;

	for(;i<current_pos+18*12;i++)
	{
		LOGI("buf[%d]: %d", i, buf[i]);
		if(buf[i] > HIGH_GATE) //32767-8192
		{
			j++;
		}
		else
		{
			j=0;
		}
		if(j>=18*4) //2个0xFF，至少14位1
		{
			return 1;
		}
	}
	LOGI("not frame end j: %d", j);
	return 0;
}

int find_start_point(int start, int end)
{
	LOGI("find_start_point --> start at: %d  end at: %d", start, end);
	int i=start,j=0,k=end;
	signed short *buf = input;
	for(;i<k;i+=18)
	{
		if(buf[i] < 8192 && buf[i] > -8192) 
		{
			j++;
		}
		else
		{
			j=0;
		}
		if(j>=40)     
		{
			LOGI("    --> find_start_point at: %d",i);
			current_pos = i;
			return 1;
		}
	}
	LOGE("Not found start point");
	return 0;
}


int find_data_start(int start, int end)
{
	LOGI("find_data_start --> start at: %d  end at: %d", start, end);
	int i=start,j=0,k=end;
	signed short *buf = input;
	for(;i<k;i++)
	{
		if(buf[i] > HIGH_GATE) //32767-8192
		{
			j++;
		}
		else
		{
			j=0;
		}
		if(j>=18*8) //2个0xFF，至少14位1
		{
			while(i<k)
			{
				i++;
				if((buf[i] < LOW_GATE) && (buf[i-1] < LOW_GATE) && (buf[i-2] < LOW_GATE))
					break;
			}
			current_pos = i-2;

			if(buf[current_pos+4*18+9] < LOW_GATE)
			{
				LOGI("    --> this is a 0xaa start: current_pos: %d", current_pos);
				return -1;
			}
			else
			{
				LOGI("    --> find_data_start at: %d",current_pos);
				return current_pos;
			}
		}
	}
	LOGI("    --> find_data_start failed (end at %d)", i-2);
	return 0;
}

int find_aa_start(int start, int end)
{
	LOGI("find_aa_start --> start at: %d  end at: %d", start, end);
	int i=start,k=end;
	signed short *buf = input;
	
	for(;i<k;i++)
	{
		if((buf[i] > HIGH_GATE) && (buf[i-1] > HIGH_GATE) && (buf[i-2] > HIGH_GATE))
		{
			LOGI("    find_aa_start at: %d",i-2);
			current_pos = i-2;
			//找到0xaa起始位置，返回成功
			return 1;//break;
		}
	}
	LOGI("    find_aa_start failed (end at %d)", i-2);
	return 0;
}

int find_chars(const char *in, char *out, int count)
{
	LOGD("find_chars");

	int a=0, b=0, i=0;

	for(i=0; i<count; i++)
	{
		if(in[i] == '#' && in[i-1] == '#')
		{
			a = i;
			break;
		}
	}

	for(; i<count; i++)
	{
		if(in[i] == '@' && in[i-1] == '@')
		{
			b = i-1;
			break;
		}
	}

	LOGD("    --> a: %d   b: %d  i: %d", a, b, i);

	if((b-a) >= 2 && (b-a) <= 253 )
	{
		LOGD("    --> find chars");
		memcpy(out, &in[a+1], b-a-1);
		return b-a-1;
	}
	else
	{
		LOGD("    --> not find");
		return 0;
	}
}

int analysis_data(int start, int end)
{
	LOGD("analysis_data start:  %d    end: %d",start,end);
	current_pos = 0;
	int i=0, success=0, frames=0;

	while(current_pos < end)
	{
		int find=0;
#if 0
start:
		find = find_start_point(current_pos, end);
		if(find) //找到起始长时间低电平
		{
			frames++;
			//找到0xaa起始位置
			if(find_aa_start(current_pos,end))
			{
#endif
find:
				find = find_data_start(current_pos,end);
				if(find == 0)
				{
					return 0;
				}
				else if(find == -1)
				{
					goto find;
				}
				else
				{
					//循环读取字节数据，有错误，重新开始下一frame数据分析
					do
					{
						if(end-current_pos < 18*26)
						{
							LOGI("no enough datas, success: %d",success);
							return 0;
						}
						if(!read_byte(&temp[i],end))
						{
							i=0;
							goto find;
						}
						LOGI("DATA[%d], 0x%x", i, temp[i]);
						i++;
						if(i==256)
							break;
					}while(!is_one_frame_end(end));

					int ret;
					if(ret = find_chars(temp, buf_out, i))
					{
						return ret; //如果查找成功，
					}
					else
					{
						i=0;
						goto find;

					}
				}
#if 0
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
#endif
	}
}

int receive(int timeout)
{
	LOGI("receive");

	int i, ret, total=0;
	signed char *buf = input;
	int count = DATA_LEN/(my_audio_struct.input_config.buffer_size*20);

	if (ioctl(my_audio_struct.fd_input, AUDIO_START, 0)) {
         	perror("cannot start audio");
        	goto fail;
     	}

     	fcntl(0, F_SETFL, O_NONBLOCK);

	struct timeval tv_start;
	gettimeofday(&tv_start, NULL);
	LOGD("time-start %u:%u\n", tv_start.tv_sec, tv_start.tv_usec);

	LOGD("    --> count: %d", count);
	for(i=0; i<count; i++)
	{
		struct timeval tv_end;
		gettimeofday(&tv_end, NULL);
		LOGD("time-end %u:%u\n", tv_end.tv_sec, tv_end.tv_usec);

		//超时退出
		if(tv_end.tv_sec-tv_start.tv_sec > timeout)
		{
			LOGD("timeout");
			goto timeout;
		}
	
		if (read(my_audio_struct.fd_input, buf+total, my_audio_struct.input_config.buffer_size*20) != my_audio_struct.input_config.buffer_size*20) 
		{
             		LOGE("cannot record to buffer");
           		goto fail;
        	}
		total += my_audio_struct.input_config.buffer_size*20;

		//分析数据
		ret = analysis_data(0,total/2);

		if(ret > 0)
		{
			goto success;
		}
		else if(ret == 0)
		{
			LOGD("    --> continue");
			continue;
		}
	}

fail:
	ret = 0;	
	ioctl(my_audio_struct.fd_input, AUDIO_STOP, 0);
	//close(my_audio_struct.fd_input);
	return ret;
timeout:
	ret = -1;
success:
	ioctl(my_audio_struct.fd_input, AUDIO_STOP, 0);
	//close(my_audio_struct.fd_input);
	return ret;
}

int test_recordData()
{
	signed char *buf = input;
    	unsigned total = 0, cur = 0,i;

     	if (ioctl(my_audio_struct.fd_input, AUDIO_START, 0)) {
         	perror("cannot start audio");
        	goto fail;
     	}

     	fcntl(0, F_SETFL, O_NONBLOCK);
	
	//循环录制数据到缓冲区
	for(;;)
	{
		if(total<44100*2*10)
		{  
			int diff = 44100*2*10-total;
			if(diff < my_audio_struct.input_config.buffer_size*2)
			{
				break;
			}
			else
			{
				if (read(my_audio_struct.fd_input, buf+total, my_audio_struct.input_config.buffer_size*2) != my_audio_struct.input_config.buffer_size*2) 
				{
             				LOGE("cannot read record buffer2");
             				goto fail;
        			}
			}
			total += my_audio_struct.input_config.buffer_size*2;
			cur = total;
		}
		else
		{
			goto done;
		}
	}
done:
	ioctl(my_audio_struct.fd_input, AUDIO_STOP, 0);
     	close(my_audio_struct.fd_input);

        /* update lengths in header */
     	return 0;

fail:
     	return -1;
}

int do_Rec()
{
    struct wav_header hdr;
    unsigned char buf[8192];
    struct msm_audio_config cfg;
    unsigned sz, n;
    int fd, afd;
    unsigned total = 0;
    unsigned char tmp;
    
    hdr.riff_id = ID_RIFF;
    hdr.riff_sz = 0;
    hdr.riff_fmt = ID_WAVE;
    hdr.fmt_id = ID_FMT;
    hdr.fmt_sz = 16;
    hdr.audio_format = FORMAT_PCM;
    hdr.num_channels = 1;
    hdr.sample_rate = 44100;
    hdr.byte_rate = hdr.sample_rate * hdr.num_channels * 2;
    hdr.block_align = hdr.num_channels * 2;
    hdr.bits_per_sample = 16;
    hdr.data_id = ID_DATA;
    hdr.data_sz = 0;

    fd = open("/sdcard/out.txt", O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("cannot open output file");
        return -1;
    }
    write(fd, &hdr, sizeof(hdr));

    afd = open("/dev/msm_pcm_in", O_RDWR);
    if (afd < 0) {
        perror("cannot open msm_pcm_in");
        close(fd);
        return -1;
    }

    printf("RIFF_ID2: 0x%x\n",hdr.riff_id);
        /* config change should be a read-modify-write operation */
    if (ioctl(afd, AUDIO_GET_CONFIG, &cfg)) {
        perror("cannot read audio config");
        goto fail;
    }

    printf("RIFF_ID3: 0x%x\n",hdr.riff_id);
    cfg.channel_count = hdr.num_channels;
    cfg.sample_rate = hdr.sample_rate;
    if (ioctl(afd, AUDIO_SET_CONFIG, &cfg)) {
        perror("cannot write audio config");
        goto fail;
    }

    printf("RIFF_ID4: 0x%x\n",hdr.riff_id);
    if (ioctl(afd, AUDIO_GET_CONFIG, &cfg)) {
        perror("cannot read audio config");
        goto fail;
    }

    sz = cfg.buffer_size;
    fprintf(stderr,"buffer size %d x %d\n", sz, cfg.buffer_count);
    if (sz > sizeof(buf)) {
        fprintf(stderr,"buffer size %d too large\n", sz);
        goto fail;
    }

    if (ioctl(afd, AUDIO_START, 0)) {
        perror("cannot start audio");
        goto fail;
    }

    fcntl(0, F_SETFL, O_NONBLOCK);
    fprintf(stderr,"\n*** RECORDING * HIT ENTER TO STOP ***\n");

    for (;;) {
        while (read(0, &tmp, 1) == 1) {
            if ((tmp == 13) || (tmp == 10))
	    {
		   printf("Now Done\n");
		   goto done;
	    }
        }
        if (read(afd, buf, sz) != sz) {
            perror("cannot read buffer");
            goto fail;
        }
        if (write(fd, buf, sz) != sz) {
            perror("cannot write buffer");
            goto fail;
        }
        total += sz;
	printf("total = %d\n",total);

    }
done:
    printf("Exit record with total = %d\n",total);
    close(afd);

        /* update lengths in header */
    printf("RIFF_ID: 0x%x\n",hdr.riff_id);
    int *temp = (int *) (&hdr);
    int i;
    for (i=0;i<11;i++)
    	printf("--> 0x%x\n",*(temp++));

    /////  ADD BY WEI  /////
    hdr.riff_id = ID_RIFF;
    
    hdr.data_sz = total;
    hdr.riff_sz = total + 8 + 16 + 8;
    lseek(fd, 0, SEEK_SET);
    int ret_val = write(fd, &hdr, sizeof(hdr));
    printf("ret_val = %a\n",ret_val);
    printf("Now close wav file\n");
    close(fd);
    printf("Done~\n");
    return 0;

fail:
    printf("Failed~\n");
    close(afd);
    close(fd);
    return -1;
}

int test_doRec()
{
    LOGI("test_doRec");
    struct wav_header hdr;
    unsigned char buf[8192];
    struct msm_audio_config cfg;
    unsigned sz, n;
    int fd, afd;
    unsigned total = 0;
    unsigned char tmp;
    



    if (ioctl(my_audio_struct.fd_input, AUDIO_START, 0)) {
        perror("cannot start audio");
        goto fail;
    }

    fcntl(0, F_SETFL, O_NONBLOCK);
    fprintf(stderr,"\n*** RECORDING * HIT ENTER TO STOP ***\n");

    for (;;) {
        while (read(0, &tmp, 1) == 1) {
            if ((tmp == 13) || (tmp == 10)) goto done;
        }
        if (read(afd, buf, sz) != sz) {
            perror("cannot read buffer");
            goto fail;
        }
        if (write(fd, buf, sz) != sz) {
            perror("cannot write buffer");
            goto fail;
        }
        total += sz;

    }
done:
    close(my_audio_struct.fd_input);

        /* update lengths in header */
    hdr.data_sz = total;
    hdr.riff_sz = total + 8 + 16 + 8;
    lseek(fd, 0, SEEK_SET);
    write(fd, &hdr, sizeof(hdr));
    close(fd);
    return 0;

fail:
    close(afd);
    close(fd);
    return -1;
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_unitInit(JNIEnv *env, jobject thiz, jint play_rate, jint play_channels, jint rec_rate, jint rec_channels, jint flags)
{
 	return unit_init(play_rate, play_channels, rec_rate, rec_channels, flags);
}

void Java_com_thinpad_audiotransfer_AudiotransferActivity_unitDestroy()
{
	unit_destroy();
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_testSend()
{
	test_doSend();
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_transferOneFrame()
{
	test_transferOneFrame();
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_sendMessage(JNIEnv *env, jobject thiz, jbyteArray bytes, jint length)
{
	//char *tmp="test";

	//获取消息内容
	const jbyte* tmp = (*env)->GetByteArrayElements(env,bytes,JNI_FALSE);

	msg_len = length;	 
	GET_ARRAY_LEN(head_out,head_len);
	GET_ARRAY_LEN(tail_out,tail_len);

	//计算总消息长度,消息长度＊2为了校验
	sum = msg_len;

	char *buf;
	buf = (char *)malloc(sum);
	memset(buf, 0xff, sum);

 	int len = make_message(buf, tmp, msg_len);

	my_audio_struct.msg = buf;
	send_message(&my_audio_struct, len);

	free(buf);
	my_audio_struct.msg = NULL;

	return 0;
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_testRecordData()
{
	test_recordData();
	//do_Rec();
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_testSaveData()
{
	int fd = open("/sdcard/out.txt", O_CREAT | O_RDWR, 0666);
	if (fd < 0) {
         	LOGE("cannot open out file");
        	return -1;
    	}
	if (write(fd, input, 44100*2*10) != 44100*2*10) {
             	LOGE("cannot write buffer");
           return -1;
        }
	LOGI("Output to file success");
	close(fd);
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_receiveData(JNIEnv *env, jobject thiz, jbyteArray recv, int timeout)
{
	int ret = receive(timeout);
	jbyte* tmp = (*env)->GetByteArrayElements(env,recv,JNI_FALSE);
	if(ret > 0)
	{
		memcpy(tmp, buf_out, ret);
	}
	return ret;
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_receive()
{	
	
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_testReadFile()
{	
#if 0
	int fd = open("/sdcard/in.txt", O_RDONLY);
	if (fd < 0) {
         	LOGE("cannot open in file");
        	return -1;
    	}

	if (read(fd, input, DATA_LEN) != DATA_LEN) {
             	LOGE("cannot read buffer");
           return -1;
        }
	LOGI("Read input file success");
	close(fd);
#endif	

	int ret = receive(9);
	
	int j;
	for(j=0; j<ret; j++)
	{
		LOGD("buf_out[%d] = %d", j, buf_out[j]);
	}
	return ret;
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_createThread()
{
	pthread_create(NULL, NULL, &test_recordData, NULL);
	LOGI("Read thread create success");
}

#if 1 //For test only
int main()
{
	unit_init(44100,2,44100,2,3);
	//test_doSend();
	test_doRec();
}
#endif
