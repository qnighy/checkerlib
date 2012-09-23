///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////////                       checkerlib.h                            ////////
////////  An I/O library for checking correctness of program/data.     ////////
////////                                                               ////////
////////  Author: Masaki Hara                                          ////////
////////  License: GPL-3.0 or later                                    ////////
////////                                                               ////////
////////                                                               ////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifndef CHECKERLIB_H
#define CHECKERLIB_H

#if !defined(__cplusplus)
#error "checkerlib.h supports only C++."
#endif

#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

////
//// platform-independence
////

#if defined(_WIN32) && !defined(__unix__)
#include <Windows.h>

inline int err(int c,const char *pszAPI)
{
  perror(pszAPI);
  exit(c);
  return c;
}
#else
#include <err.h>
#endif

////
//// compiler-independence
////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlong-long"

#define ATTR_PRINTF(x,y) __attribute__ ((format (printf, x, y)))
#define ATTR_NORETURN __attribute__((noreturn))

#else

#define ATTR_PRINTF(x,y)
#define ATTR_NORETURN

#endif

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>

namespace checker {
  // integer constants for parsing
  const int INT_MAX_DECIMAL_U = 214748364;
  const int INT_MAX_DECIMAL_L = 7;
  const int INT_MIN_DECIMAL_U = -214748364;
  const int INT_MIN_DECIMAL_L = -8;
  const long long LLONG_MAX_DECIMAL_U = 922337203685477580LL;
  const int LLONG_MAX_DECIMAL_L = 7;
  const long long LLONG_MIN_DECIMAL_U = -922337203685477580LL;
  const int LLONG_MIN_DECIMAL_L = -8;

  // vsprintf & sprintf for std::string
  inline std::string string_vsprintf(const char *format, va_list ap) {
    // TODO: ap can only be used once in C89 (C99: can use va_copy)
    char *c_str = new char[200];
    int retval = vsnprintf(c_str, 200, format, ap);
    if(retval < 0) {
      throw std::runtime_error("vsnprintf() failed\n");
    }
    std::string str(c_str);
    delete[] c_str;
    return str;

    /* int retval = vsprintf(NULL, format, ap);
    if(retval < 0) {
      throw std::runtime_error("vsprintf() failed\n");
    }
    char *c_str = new char[retval];
    int retval2 = vsprintf(c_str, format, ap);
    if(retval2 < 0) {
      throw std::runtime_error("vsprintf() failed\n");
    }
    std::string str(c_str);
    delete[] c_str;
    return str; */
  }
  inline std::string string_sprintf(const char *format, ...) ATTR_PRINTF(1,2);
  inline std::string string_sprintf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    std::string str = string_vsprintf(format,ap);
    va_end(ap);
    return str;
  }

  // exception
  class ParseError : public std::runtime_error {
  public:
    ParseError(const std::string& what) : std::runtime_error(what) {}
  };


  class ReaderImpl {
    friend class Reader;
    friend class TokenData;
    int ref_cnt;
    FILE *internal_fp;
    char *filename;
    int lastchar;
    int line,col;
    ReaderImpl(FILE *internal_fp, const char *filename)
      : ref_cnt(0),
        internal_fp(internal_fp),
        filename(new char[strlen(filename)+1]),
        lastchar(-1),line(1),col(0) {
      strcpy(this->filename, filename);
    }
    ~ReaderImpl() {
      if(internal_fp) {
        throw std::logic_error(string_sprintf("%s: call readEof() or abortReading() before disposing!", filename));
      }
      delete[] filename;
    }
    std::string positionDescription() {
      return string_sprintf("%s(%d,%d): ", filename, line, col);
    }
    int readChar() {
      int ret = fgetc(internal_fp);
      if(ferror(internal_fp)) {
        throw std::runtime_error(string_sprintf("error reading file: %s", strerror(errno)));
      }
#if defined(_WIN32)
      if(ret=='\r') {
        ret = fgetc(internal_fp);
        if(ferror(internal_fp)) {
          throw std::runtime_error(string_sprintf("error reading file: %s", strerror(errno)));
        }
      }
#endif
      if(lastchar=='\n'){line++,col=1;}else{col++;}
      lastchar = ret;
      return ret;
    }
    void abortReading() {
      internal_fp = NULL;
    }
    void abortReadingWithError(std::string str) ATTR_NORETURN {
      abortReading();
      throw ParseError(positionDescription()+str);
    }

    template<typename T>
    class DelimiterData {
      friend class ReaderImpl;
      ReaderImpl* reader;
      int delim;
      T data;
      DelimiterData(ReaderImpl* reader, int delim, T data)
        : reader(reader), delim(delim), data(data) {}
    public:
      int getDelim() const { return this->delim; }
      T getData() const { return this->data; }
      T eol() const {
        if(delim!='\n') {
          this->reader->abortReadingWithError("delimiter EOL is expected");
        }
        return data;
      }
      T spc() const {
        if(delim!=' ') {
          this->reader->abortReadingWithError("delimiter SPC is expected");
        }
        return data;
      }
      T ary(int i, int n) const {
        if(i<0 || i >=n) {
          throw std::invalid_argument("DelimiterData::ary(int,int): invalid condition");
        }
        if(i+1==n) {
          return this->eol();
        } else {
          return this->spc();
        }
      }
    };

    template<typename T>
    class NumData : public DelimiterData<T> {
      friend class ReaderImpl;
      NumData(ReaderImpl* reader, int delim, T data)
        : DelimiterData<T>(reader,delim,data) {}
    public:
      NumData range(T min_val, T max_val) const {
        if(! (min_val <= this->getData() && this->getData() <= max_val) ) {
          this->reader->abortReadingWithError("invalid range");
        }
        return *this;
      }
    };

    NumData<int> readIntV(const char *format, va_list ap) {
      int i;
      int c = readChar();
      if(c == '-') {
        c = readChar();
        if('1' <= c && c <= '9') {
          i = - (c - '0');
          for(;;) {
            c = readChar();
            if('0' <= c && c <= '9') {
              if((i == INT_MIN_DECIMAL_U && - (c - '0') < INT_MIN_DECIMAL_L) || i < INT_MIN_DECIMAL_U) {
                abortReadingWithError(
                    "error reading int "+
                    string_vsprintf(format,ap)+
                    ": Too large integer constant");
              }
              i = i*10 - (c - '0');
            } else {
              return NumData<int>(this, c, i);
            }
          }
        }
      } else if(c == '0') {
        i = 0;
        c = readChar();
        return NumData<int>(this, c, i);
      } else if('1' <= c && c <= '9') {
        i = (c - '0');
        for(;;) {
          c = readChar();
          if('0' <= c && c <= '9') {
            if((i == INT_MAX_DECIMAL_U && (c - '0') > INT_MAX_DECIMAL_L) || i > INT_MAX_DECIMAL_U) {
              abortReadingWithError(
                  "error reading int "+
                  string_vsprintf(format,ap)+
                  ": Too large integer constant");
            }
            i = i*10 + (c - '0');
          } else {
            return NumData<int>(this, c, i);
          }
        }
      }
      abortReadingWithError(
          "error reading int "+
          string_vsprintf(format,ap)+
          ": not an integer input");
    }

    NumData<long long> readLongV(const char *format, va_list ap) {
      long long i;
      int c = readChar();
      if(c == '-') {
        c = readChar();
        if('1' <= c && c <= '9') {
          i = - (c - '0');
          for(;;) {
            c = readChar();
            if('0' <= c && c <= '9') {
              if((i == LLONG_MIN_DECIMAL_U && - (c - '0') < LLONG_MIN_DECIMAL_L) || i < LLONG_MIN_DECIMAL_U) {
                abortReadingWithError(
                    "error reading long long "+
                    string_vsprintf(format,ap)+
                    ": Too large integer constant");
              }
              i = i*10 - (c - '0');
            } else {
              return NumData<long long>(this, c, i);
            }
          }
        }
      } else if(c == '0') {
        i = 0;
        c = readChar();
        return NumData<long long>(this, c, i);
      } else if('1' <= c && c <= '9') {
        i = (c - '0');
        for(;;) {
          c = readChar();
          if('0' <= c && c <= '9') {
            if((i == LLONG_MAX_DECIMAL_U && (c - '0') > LLONG_MAX_DECIMAL_L) || i > LLONG_MAX_DECIMAL_U) {
              abortReadingWithError(
                  "error reading long long "+
                  string_vsprintf(format,ap)+
                  ": Too large integer constant");
            }
            i = i*10 + (c - '0');
          } else {
            return NumData<long long>(this, c, i);
          }
        }
      }
      abortReadingWithError(
          "error reading long long "+
          string_vsprintf(format,ap)+
          ": not an integer input");
    }

    void readEof() {
      if(readChar() != -1) {
        internal_fp = NULL;
        abortReadingWithError("error reading EOF: not an EOF");
      }
      if(ferror(internal_fp)) {
        internal_fp = NULL;
        throw std::runtime_error(string_sprintf("error reading file: %s", strerror(errno)));
      }
      if(fclose(internal_fp)==EOF) {
        internal_fp = NULL;
        throw std::runtime_error(string_sprintf("error closing file: %s", strerror(errno)));
      }
      internal_fp = NULL;
    }
  };
  class Reader {
    ReaderImpl *reader;
  public:
    void open(FILE *fp, const char *filename) {
      if(reader) throw std::domain_error("Reader::open(FILE*,const char*): already opened.");
      if(!fp) throw std::invalid_argument("Reader::open(FILE*,const char*): fp is NULL.");
      if(!filename) throw std::invalid_argument("Reader::open(FILE*,const char*): filename is NULL.");
      reader = new ReaderImpl(fp, filename);
      reader->ref_cnt++;
    }
    void open_stdin() {
      open(stdin, "<stdin>");
    }
    Reader() : reader(NULL) {}
    Reader(FILE *fp) : reader(NULL) {
      if(fp==stdin) {
        open(stdin, "<stdin>");
      } else {
        throw std::invalid_argument("Reader(FILE*): specify filename explicitly.");
      }
    }
    Reader(FILE *fp, const char *filename) : reader(NULL) {
      open(fp, filename);
    }
    Reader(const Reader& that) : reader(that.reader) {
      reader->ref_cnt++;
    }
    Reader& operator=(const Reader& that) {
      if(!--reader->ref_cnt) {
        delete reader;
      }
      reader = that.reader;
      reader->ref_cnt++;
      return *this;
    }
    ~Reader() {
      if(!--reader->ref_cnt) {
        delete reader;
      }
    }

    ReaderImpl::NumData<int> readInt(const char *format = "<?>", ...) ATTR_PRINTF(2,3) {
      va_list ap;
      va_start(ap, format);
      ReaderImpl::NumData<int> ret = reader->readIntV(format, ap);
      va_end(ap);
      return ret;
    }

    ReaderImpl::NumData<long long> readLong(const char *format = "<?>", ...) ATTR_PRINTF(2,3) {
      va_list ap;
      va_start(ap, format);
      ReaderImpl::NumData<long long> ret = reader->readLongV(format, ap);
      va_end(ap);
      return ret;
    }

    void abortReading() {
      reader->abortReading();
    }
    void abortReadingWithError(std::string message) ATTR_NORETURN {
      reader->abortReadingWithError(message);
    }
    void readEof() {
      reader->readEof();
    }
  };
}

////
//// clean-up
////

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* CHECKERLIB_H */
