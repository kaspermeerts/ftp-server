#ifndef __STREAM_H__
#define __STREAM_H__

enum stream_type
{
	S_FILE,
	S_SOCKET,
};

typedef struct stream
{
	int fd;
	int type;
} stream_t;

extern ssize_t splice_stream(stream_t , off_t *, stream_t , off_t *, size_t);
extern ssize_t sendall(int , const void *, size_t , int );

#endif
