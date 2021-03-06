#include <string.h>
#include <malloc.h>
#include <windows.h>
#include <stdio.h>

#include "ffread.hpp"

/******************/
/* ERROR HANDLING */
/******************/

#include "fail.h"
void ReportError(char const * mesg1, char const * mesg2)
{
	char * mesg;
	DWORD err = 0;
	
	if (!mesg1) mesg1="";
	
	if (mesg2)
	{
		mesg = new char [strlen(mesg1)+strlen(mesg2)+3];
		
		strcpy(mesg,mesg1);
		strcat(mesg,"\n\n");
		strcat(mesg,mesg2);
	}
	else
	{
		char * lpMsgBuf;

		err = GetLastError();
		
		FormatMessage( 
		    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		    NULL,
		    err,
		    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		    (LPTSTR) &lpMsgBuf,
		    0,
		    NULL 
		);

		mesg = new char [strlen(mesg1)+strlen(lpMsgBuf)+3];
		
		strcpy(mesg,mesg1);
		strcat(mesg,"\n\n");
		strcat(mesg,lpMsgBuf);
		
		// Free the buffer.
		LocalFree( lpMsgBuf );
	}
	
	// Display the string.
	fail("----------ERROR:%08x-----------\n\n%s\n\n",err,mesg);
   
   	// Free the buffer.
	delete[] mesg;
}

/***************/
/* HANDY MACRO */
/***************/

#define READ_FILE(fname,post_proc,on_return,h,data,n_bytes,n_bytes_read,p5) \
	if (!ReadFile(h,data,n_bytes,&n_bytes_read,p5)) \
	{ \
		ReportError(fname); \
		post_proc; \
		on_return; \
		return FF_COULDNOTWRITEFILE; \
	} \
	else if (n_bytes_read != n_bytes) \
	{ \
		ReportError(fname,"Could not write the correct number of bytes"); \
		post_proc; \
		on_return; \
		return FF_COULDNOTWRITEFILE; \
	} \
	else post_proc;
	

/*****************/
/* class FFDataI */
/*****************/

FFDataI::FFDataI(char const *_filename, void *_data, size_t _length)
: filename(0)
, data(_data)
, length(_length)
{
	if (_filename)
	{
		filename = new char [strlen(_filename)+1];
		strcpy(filename,_filename);
	}
}

FFDataI::FFDataI(FFDataI const & ffd, ptrdiff_t offset)
: filename(0)
, data((void *)((size_t)ffd.data + offset))
, length(ffd.length)
{
	if (ffd.filename)
	{
		filename = new char [1+strlen(ffd.filename)];
		strcpy(filename,ffd.filename);
	}
}

FFDataI & FFDataI::operator = (FFDataI const & ffd)
{
	if (&ffd != this)
	{
		if (filename) delete[] filename;
		
		filename = 0;
		data = ffd.data;
		length = ffd.length;
		
		if (ffd.filename)
		{
			filename = new char [1+strlen(ffd.filename)];
			strcpy(filename,ffd.filename);
		}
	}
	return *this;
}

FFDataI::~FFDataI()
{
	if (filename) delete[] filename;
}

int FFDataI::operator == (FFDataI const & ffd) const
{
	return ! _stricmp(filename ? filename : "", ffd.filename ? ffd.filename : "");
}

int FFDataI::operator < (FFDataI const & ffd) const
{
	return _stricmp(filename ? filename : "", ffd.filename ? ffd.filename : "") < 0;
}

int FFDataI::operator == (char const * name) const
{
	return ! _stricmp(filename ? filename : "", name ? name : "");
}

int FFDataI::operator < (char const * name) const
{
	return _stricmp(filename ? filename : "", name ? name : "") < 0;
}

/*******************/
/* class FFHeaderI */
/*******************/

int FFHeaderI::HashFunction(char const * nam)
{
	int v = 0;

	while (*nam) v += toupper(*nam++);

	return v & (FFHI_HASHTABLESIZE-1);
}

FFHeaderI::FFHeaderI(char const *_filename,BOOL _should_be_kept)
: filename(0)
, data(0)
, length(0)
,should_be_kept(_should_be_kept)
{
	if (_filename)
	{
		filename = new char [strlen(_filename)+1];
		strcpy(filename,_filename);
		
		Read();
	}
}

FFHeaderI::FFHeaderI(FFHeaderI const & ffh)
: filename(0)
, data(0)
, length(ffh.length)
,should_be_kept(ffh.should_be_kept)
{
	if (ffh.filename)
	{
		filename = new char [1+strlen(ffh.filename)];
		strcpy(filename,ffh.filename);
	}
	if (ffh.data)
	{
		data = malloc(length);
		memcpy(data,ffh.data,length);
	}
	ptrdiff_t offset = (size_t)data - (size_t)ffh.data;
	for (int i=0; i<FFHI_HASHTABLESIZE; ++i)
	{
		for (CLIF<FFDataI> i_file(&ffh.files[i]); !i_file.done(); i_file.next())
		{
			files[i].add_entry(FFDataI(i_file(),offset));
		}
	}
}

FFHeaderI & FFHeaderI::operator = (FFHeaderI const & ffh)
{
	if (&ffh != this)
	{
		if (data) free(data);
		if (filename) delete[] filename;
		
		filename = 0;
		data = 0;
		length = ffh.length;
		
		if (ffh.filename)
		{
			filename = new char [1+strlen(ffh.filename)];
			strcpy(filename,ffh.filename);
		}
		if (ffh.data)
		{
			data = malloc(length);
			memcpy(data,ffh.data,length);
		}
		ptrdiff_t offset = (size_t)data - (size_t)ffh.data;
		for (int i=0; i<FFHI_HASHTABLESIZE; ++i)
		{
			for (CLIF<FFDataI> i_file(&ffh.files[i]); !i_file.done(); i_file.next())
			{
				files[i].add_entry(FFDataI(i_file(),offset));
			}
		}
	}
	return *this;
}

FFHeaderI::~FFHeaderI()
{
	if (filename) delete[] filename;
	if (data) free(data);
}

int FFHeaderI::operator == (FFHeaderI const & ffh) const
{
	return ! _stricmp(filename ? filename : "", ffh.filename ? ffh.filename : "");
}

void FFHeaderI::Clear()
{
	if (data) free(data);
	data = 0;
	length = 0;
	
	for (int i=0; i<FFHI_HASHTABLESIZE; ++i)
	{
		while (files[i].size())
			files[i].delete_first_entry();
	}
}

FFError FFHeaderI::Read(char const *_filename)
{
	if (_filename)
	{
		if (filename) delete[] filename;
		filename = new char [strlen(_filename)+1];
		strcpy(filename,_filename);
	}
	
	HANDLE h = CreateFile (filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	
	if (INVALID_HANDLE_VALUE==h)
	{
		ReportError(_filename);
		return FF_COULDNOTOPENFILE;
	}
	
	// reset vars
	Clear();
	
	char magic[4];
	unsigned long rffl_version;
	size_t num_files;
	size_t total_headsize;
	
	DWORD bytes_read;
	
	READ_FILE(filename,(void)0,CloseHandle(h),h,magic,4,bytes_read,0)
	READ_FILE(filename,(void)0,CloseHandle(h),h,&rffl_version,4,bytes_read,0)
	READ_FILE(filename,(void)0,CloseHandle(h),h,&num_files,4,bytes_read,0)
	READ_FILE(filename,(void)0,CloseHandle(h),h,&total_headsize,4,bytes_read,0)
	READ_FILE(filename,(void)0,CloseHandle(h),h,&length,4,bytes_read,0)
	
	if (strncmp(magic,"RFFL",4))
	{
		ReportError(filename,"Incorrect file type");
		CloseHandle(h);
		return FF_COULDNOTREADFILE;
	}
	if (rffl_version>0)
	{
		ReportError(filename,"Version not supported");
		CloseHandle(h);
		return FF_COULDNOTREADFILE;
	}
	
	void * header = malloc(total_headsize);
	
	READ_FILE(filename,(void)0,CloseHandle(h),h,header,total_headsize,bytes_read,0)
	
	data = malloc(length);
	
	READ_FILE(filename,(void)0,CloseHandle(h),h,data,length,bytes_read,0)
	
	CloseHandle(h);
	
	// now parse the header
	
	void * headerP = header;
	
	for (int i=0; i<num_files; ++i)
	{
		char const * fnameP = (char *)((size_t)headerP + 8);
		size_t leng = *(size_t *)((size_t)headerP + 4);
		void * dataP = (void *)((size_t)data + *(size_t *)headerP);
		
		files[HashFunction(fnameP)].add_entry(FFDataI(fnameP,dataP,leng));
		
		// increment pointer
		headerP = (void *)((size_t)headerP + 8 + strlen(fnameP) +4&~3);
	}
	
	free(header);
	
	return FF_OK;
}

void const * FFHeaderI::FindFile(char const * name, size_t * lengthP) const
{
	for (CLIF<FFDataI> i_file(&files[HashFunction(name)]); !i_file.done(); i_file.next())
	{
		if (i_file()==name)
		{
			if (lengthP) *lengthP = i_file().GetLength();
			return i_file().GetDataPointer();
		}
	}
	return 0;
}

