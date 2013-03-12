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

#include <jni.h>

#include <android/log.h>

#define SECONDS 10
#define INPUT_DEVNAME "/dev/msm_pcm_in"
#define OUTPUT_DEVNAME "/dev/msm_pcm_out"

#define LOG_TAG "WEI:jni "
#ifdef __DEBUG
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)a
#else
#define LOGI(...) //
#define LOGE(...) //
#define LOGD(...) //
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

	char *msg;
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
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
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

char input[44100*2*10];
char output[480000]; //实际输出数据
unsigned out_index = 0; //实际输出索引
unsigned out_count = 0; //实际输出计数

char *prefill; 	//预填充缓冲区
int head_len=0; //消息头长度
int tail_len=0; //消息尾长度
int msg_len=0; 	//消息长度
int sum=0; 	//消息字节总数

int open_files(unsigned flags)
{
	int fd_in, fd_out;
	LOGI("open_file --> flags: %d",flags);
	if(flags & INPUT_FLAG)
	{
		fd_in = open(INPUT_DEVNAME,O_RDWR);
		if(fd_in < 0)
		{
			LOGE("Cannot open input file");
			goto input_failed;		
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
			goto output_failed;		
		}
		LOGI("Open output success");
		my_audio_struct.fd_output = fd_out;
	}
	return 0;

output_failed:
	if(fd_out)
	{
		LOGI("Close /dev/msm_pcm_out");
		close(fd_out);
	}
input_failed:
	return -1;
}

int set_params(unsigned play_rate, unsigned play_channels, unsigned rec_rate, unsigned rec_channels, unsigned flags)
{
	if(flags & INPUT_FLAG)
	{
				
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

	if(open_files(flags) != 0)
		return -1;

	if(set_params(play_rate, play_channels, rec_rate, rec_channels, flags) != 0)
		return -1;

	LOGI("Init success\n");
	return 1;
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
			LOGI("1");
			for(j=0;j<36;j++)
			{
				output[out_index] = bit0[j];
				out_index++;
			}
		}
		else
		{
			LOGI("0");
			for(j=0;j<36;j++)
			{
				output[out_index] = bit1[j];
				out_index++;
			}
		}
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

int make_message(char *dest, char *msg, int msg_len)
{
	LOGI("make_message");

	memcpy(dest, head_out, head_len);
	memcpy(dest+head_len, msg,msg_len);
	memcpy(dest+head_len+msg_len, tail_out, tail_len);

#if 1	
	LOGI("head_len:%d",head_len);
	LOGI("msg_len:%d",msg_len);
	LOGI("tail_len:%d",tail_len);
	
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

	int n=0;
	for(n=0; n<aud->output_config.buffer_count; n++)
	{
		if(write(aud->fd_output, prefill, aud->output_config.buffer_size) != aud->output_config.buffer_size)
		{
			LOGI("Prefill failed");
			break;
		}
	}

	ioctl(aud->fd_output, AUDIO_START, 0);
	int i;

	for(i=0; i<out_count/aud->output_config.buffer_size; i++)
	{
		LOGI("Sending ...");
		if(write(aud->fd_output, temp, aud->output_config.buffer_size) != aud->output_config.buffer_size)
		{
			LOGI("Send failed");
			break;
		}
		temp += aud->output_config.buffer_size;
	}
	LOGI("Send success");
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

void send_message(struct audio_struct *aud, const int count)
{
	
	int i,j;
	LOGI("count = %d",count);

	out_index=0;

	for(i=0;i<count;i++)
	{
		LOGI("msg[%d]: 0x%x", i, aud->msg[i]);
		byte2chars(aud->msg[i]);
	}
	
	transform_message(aud);

	//设置预填充缓冲区为输出’0‘
	
	int prefill_size = my_audio_struct.output_config.buffer_count*my_audio_struct.output_config.buffer_size;

	if(!prefill)
		prefill = malloc(prefill_size);

	for(i=0; i<prefill_size; i++)
	{
		for(j=0;j<36;j++)
		{
			prefill[i] = bit1[j];
		}
	}

	do_send(aud);
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_unitInit(JNIEnv *env, jobject thiz, jint play_rate, jint play_channels, jint rec_rate, jint rec_channels, jint flags)
{
 	return unit_init(play_rate, play_channels, rec_rate, rec_channels, flags);
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

	//计算总消息长度
	sum = head_len + tail_len + msg_len;

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

#if 1 //For test only
int main()
{
	unit_init(44100,2,44100,2,3);
	test_doSend();
}
#endif
