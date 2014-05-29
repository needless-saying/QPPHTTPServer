/* Copyright (C) 2012 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once

#ifndef _FIFOBUFFER_HEADER_PROTECT_
#define _FIFOBUFFER_HEADER_PROTECT_

/*
 * 缓冲区工具类
 * 管理一块内存(外部固定长度的或者内部分配自增长的),提供类似标准文件IO的接口.
*/

class Buffer
{
private:
	bool _innerBuf;		// 缓冲区是否是内部分配(如果是析构时要释放)
	char *_buffer;		// 缓冲区地址
	size_t _bufLen;		// 缓冲区长度
	size_t _pos;		// 游标位置
	size_t _len;		// 有效长度
	size_t _maxSize;	// 最大长度
	size_t _memInc;		// 内存增长幅度
	bool _readOnly;		// 只读

	size_t space();
	size_t reserve(size_t s);

	/* 
	* 禁止拷贝,内存指针引用的问题不好处理,目前提供的实现是
	* 总是新分配内存然后复制. 总觉得不是很好.
	*/
	Buffer(const Buffer &rh);
	Buffer& operator = (const Buffer &rh);
	Buffer& assign(const Buffer& rh);
public:
	/*
	* 两个构造函数分别用于自分配内存和指定内存
	*
	*/
	Buffer(size_t memInc = 1024, size_t maxSize = SIZE_T_MAX);
	Buffer(void* buf, size_t len);
	Buffer(const void* buf, size_t len);
	~Buffer();

	size_t read(void *buf, size_t len);
	size_t write(const void *buf, size_t len);
	int seek(long offset, int origin);
	size_t size() const;
	size_t capacity() const;
	bool eof() const;
	void trunc(bool freeBuf = true);
	size_t tell() const;
	const void* buffer() const;

	/*
	* 从当前游标的位置开始锁定内部缓冲区. 内部缓冲区被锁定后,可以直接使用指针做"写"操作,同时不要访问别的方法,解锁后继续.
	* 解锁动作的len表示外部缓冲区直接用指针"写"了 len 长的数据所以游标 _pos 也相应的增加 len.
	*/
	size_t lock(void **buf, size_t len);
	size_t unlock(size_t len);
};

#endif