/* Copyright (C) 2008 The Android Open Source Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/ioctl.h>

#include <jni.h>

#include <android/log.h>
#define LOG_TAG "WEI:jni "
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)a

#define AUDIO_IOCTL_MAGIC 'a'

#define AUDIO_START        _IOW(AUDIO_IOCTL_MAGIC, 0, unsigned)
#define AUDIO_STOP         _IOW(AUDIO_IOCTL_MAGIC, 1, unsigned)
#define AUDIO_FLUSH        _IOW(AUDIO_IOCTL_MAGIC, 2, unsigned)
#define AUDIO_GET_CONFIG   _IOR(AUDIO_IOCTL_MAGIC, 3, unsigned)
#define AUDIO_SET_CONFIG   _IOW(AUDIO_IOCTL_MAGIC, 4, unsigned)
#define AUDIO_GET_STATS    _IOR(AUDIO_IOCTL_MAGIC, 5, unsigned)

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

	unsigned play_rate;
	unsigned play_channels;
	unsigned rec_rate;
	unsigned rec_channels;
	unsigned flags;

	struct msm_audio_config input_config;
	struct msm_audio_config output_config;
	struct msm_audio_stats audio_stats;
} my_audio_struct;

/*
 *数据

 *
 *
 * 
 */
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
	0x01,0x80,0x01,0x80,
};

char bit0[] = {
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
	0x01,0x80,0x01,0x80,
};

char bit1[] = {
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
	0xff,0x7f,0xff,0x7f,
};

unsigned out_index = 0;
char output[480000];

int open_files(unsigned flags)
{
	int fd_in, fd_out;
	LOGI("open_file --> flags: %d",flags);
	if(flags & INPUT_FLAG)
	{
		fd_in = open("/dev/msm_pcm_in",O_RDWR);
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
		fd_out = open("/dev/msm_pcm_out",O_RDWR); 
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

int do_play()
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


int do_command()
{
	char *buf,*temp;

	temp = output;

	int n=0;
	int number = 44100*2*2*5;

	buf = malloc(number);
	memset(buf, 0x00, number);

	for(n=0; n<my_audio_struct.output_config.buffer_count; n++)
	{
		if(write(my_audio_struct.fd_output, buf, 4800) != 4800)
		{
			LOGI("2");
			break;
		}
	}

	
	ioctl(my_audio_struct.fd_output, AUDIO_START, 0);

	for(;;)
	{
		LOGI("Now playing");
		if(write(my_audio_struct.fd_output, temp, 4800) != 4800)
		{
			
			LOGI("Write exit");
			break;
		}
		temp += 4800;
	}
}

byte2chars(char in)
{
	int i,j;
	for(i=7;i>=0;i--)
	{
		if((in>>i)&0x1)
		{
			for(j=0;j<36;j++)
			{
				output[out_index] = bit1[j];
				out_index++;
			}
		}
		else
		{
			for(j=0;j<36;j++)
			{
				output[out_index] = bit0[j];
				out_index++;
			}
		}
	}
}

int transferOneFrame()
{
	int i;
	char command[] = {0xaa,0xab,0xff,0xff,0x00,0xff,0x00,0x00};
	for(i=0;i<8;i++)
		byte2chars(command[i]);
	do_command();
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_unitInit(JNIEnv *env, jobject thiz, jint play_rate, jint play_channels, jint rec_rate, jint rec_channels, jint flags)
{
 	return unit_init(play_rate, play_channels, rec_rate, rec_channels, flags);
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_doPlay()
{
	do_play();
}

jint Java_com_thinpad_audiotransfer_AudiotransferActivity_transferOneFrame()
{
	transferOneFrame();
}

#if 1 //For test only
int main()
{
	unit_init(44100,2,44100,2,3);
	do_play();
}
#endif
