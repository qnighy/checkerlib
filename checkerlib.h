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
#include <io.h>
#include <fcntl.h>

inline int err(int c,const char *pszAPI)
{
  perror(pszAPI);
  exit(c);
  return c;
}

inline void DisplayError(const char *pszAPI)
{
  LPVOID lpvMessageBuffer;
  CHAR szPrintBuffer[512];
  DWORD nCharsWritten;

  FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&lpvMessageBuffer, 0, NULL);

  wsprintf(szPrintBuffer,
      "ERROR: API    = %s.\n   error code = %d.\n   message    = %s.\n",
      pszAPI, GetLastError(), (char *)lpvMessageBuffer);

  WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),szPrintBuffer,
      lstrlen(szPrintBuffer),&nCharsWritten,NULL);

  LocalFree(lpvMessageBuffer);
  ExitProcess(GetLastError());
}
#else
#include <err.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

////
//// compiler-independence
////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlong-long"

#define ATTR_PRINTF(x,y) __attribute__ ((format (printf, x, y)))
#define ATTR_SCANF(x,y) __attribute__ ((format (scanf, x, y)))
#define ATTR_NORETURN __attribute__((noreturn))

#else

#define ATTR_PRINTF(x,y)
#define ATTR_SCANF(x,y)
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

  inline std::string itos(int i) {
    char c_str[12];
    c_str[11] = '\0';
    if(i>0) {
      for(int j = 10; j >= 0; j--) {
        c_str[j] = '0'+(i%10);
        i /= 10;
        if(i == 0) {
          return std::string(c_str+j);
        }
      }
    } else if(i<0) {
      for(int j = 10; j >= 0; j--) {
        if(i == 0) {
          c_str[j] = '-';
          return std::string(c_str+j);
        }
        c_str[j] = '0'-(i%10);
        i /= 10;
      }
    }
    return std::string("0");
  }

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
  /*inline std::string string_sprintf(const char *format, ...) ATTR_PRINTF(1,2);
  inline std::string string_sprintf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    std::string str = string_vsprintf(format,ap);
    va_end(ap);
    return str;
  }*/

  // exception
  class ParseError : public std::runtime_error {
  public:
    ParseError(const std::string& what) : std::runtime_error(what) {}
  };

  class ProcessError : public std::runtime_error {
  public:
    ProcessError(const std::string& what) : std::runtime_error(what) {}
  };

  class ReaderImpl {
    friend class Reader;
    friend class TokenData;
    int ref_cnt;
    FILE *internal_fp;
    char *filename;
    int lastchar;
    int line,col;
    char varname[1000];
    std::string *linecache;
    ReaderImpl(FILE *internal_fp, const char *filename)
      : ref_cnt(0),
        internal_fp(internal_fp),
        filename(new char[strlen(filename)+1]),
        lastchar(-1),line(1),col(0),
        linecache(NULL) {
      strcpy(this->filename, filename);
      strcpy(varname, "<init>");
    }
    void dispose() {
      if(internal_fp) {
        throw std::logic_error(std::string(filename)+": call readEof() or abortReading() before disposing!");
      }
    }
    ~ReaderImpl() {
      dispose();
      if(linecache) delete linecache;
      delete[] filename;
    }
    std::string positionDescription() {
      return std::string(filename)+"("+itos(line)+","+itos(col)+","+varname+"): ";
    }
    int readChar() {
      int ret = fgetc(internal_fp);
      if(ferror(internal_fp)) {
        throw std::runtime_error(std::string("error reading file: ")+strerror(errno));
      }
#if defined(_WIN32) && !defined(__unix__)
      if(ret=='\r') {
        ret = fgetc(internal_fp);
        if(ferror(internal_fp)) {
          throw std::runtime_error(std::string("error reading file: ")+strerror(errno));
        }
      }
#endif
      if(linecache) {
        if(ret=='\n') {
          fprintf(stderr, "%s<in>: %2d: %s\n", filename, line, linecache->c_str());
          linecache->clear();
        } else {
          linecache->push_back(ret);
        }
      }
      if(lastchar=='\n') {
        line++;
        col=1;
      } else {
        col++;
      }
      lastchar = ret;
      return ret;
    }
    void abortReading() {
      internal_fp = NULL;
    }
    void abortReadingWithError(const std::string& str) ATTR_NORETURN {
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

    void setVarnameV(const char *format, va_list ap) {
      vsnprintf(varname, 1000, format, ap);
    }

    NumData<int> readInt() {
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
                abortReadingWithError("error reading int: Too large integer constant");
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
              abortReadingWithError("error reading int: Too large integer constant");
            }
            i = i*10 + (c - '0');
          } else {
            return NumData<int>(this, c, i);
          }
        }
      }
      abortReadingWithError("error reading int: not an integer input");
    }

    NumData<long long> readLong() {
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
                abortReadingWithError("error reading long long: Too large integer constant");
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
              abortReadingWithError("error reading long long: Too large integer constant");
            }
            i = i*10 + (c - '0');
          } else {
            return NumData<long long>(this, c, i);
          }
        }
      }
      abortReadingWithError("error reading long long: not an integer input");
    }

    void readEof() {
      if(readChar() != -1) {
        internal_fp = NULL;
        abortReadingWithError("error reading EOF: not an EOF");
      }
      if(ferror(internal_fp)) {
        internal_fp = NULL;
        throw std::runtime_error(std::string("error reading file: ")+strerror(errno));
      }
      if(fclose(internal_fp)==EOF) {
        internal_fp = NULL;
        throw std::runtime_error(std::string("error reading file: ")+strerror(errno));
      }
      internal_fp = NULL;
    }

    void enableIODump() {
      if(!linecache) {
        linecache = new std::string();
      }
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
      reader->setVarnameV(format, ap);
      va_end(ap);
      ReaderImpl::NumData<int> ret = reader->readInt();
      return ret;
    }

    ReaderImpl::NumData<long long> readLong(const char *format = "<?>", ...) ATTR_PRINTF(2,3) {
      va_list ap;
      va_start(ap, format);
      reader->setVarnameV(format, ap);
      va_end(ap);
      return reader->readLong();
    }

    void dispose() {
      reader->dispose();
    }
    void abortReading() {
      reader->abortReading();
    }
    void abortReadingWithError(const std::string& message) ATTR_NORETURN {
      reader->abortReadingWithError(message);
    }
    void readEof() {
      reader->readEof();
    }

    void enableIODump() {
      reader->enableIODump();
    }
  };

  ////
  //// Tools for Reactive
  ////
  class ProcessImpl {
    friend class Process;
    char *procname;
    Reader& reader;
    FILE *write_file;
    FILE *read_file;
    pid_t pid;
    int ref_cnt;
    int line;
    std::string *linecache;
    char *prtcache;
    ProcessImpl(const char *procname,Reader& reader, FILE *write_file, FILE *read_file,
        pid_t pid)
      : procname(new char[strlen(procname)+1]), reader(reader), write_file(write_file),
        read_file(read_file), pid(pid), ref_cnt(0), line(1), linecache(NULL), prtcache(NULL) {
      strcpy(this->procname, procname);
      reader.open(read_file, procname);
    }
    void closeProcess() {
      fclose(write_file);
      reader.dispose();
      int status;
      waitpid(pid, &status, WUNTRACED);
      if(!WIFEXITED(status)) {
        throw ProcessError(std::string(procname)+": exited abnormally");
      }
      if(WEXITSTATUS(status) != 0) {
        throw ProcessError(std::string(procname)+": exited with status "+itos(WEXITSTATUS(status)));
      }
      write_file = read_file = NULL;
    }
    ~ProcessImpl() {
      if(write_file) {
        closeProcess();
      }
      if(linecache) {
        delete linecache;
        delete[] prtcache;
      }
      delete[] procname;
    }
    int vscanf(const char *format, va_list ap) {
      return vfscanf(read_file, format, ap);
    }
    int vprintf(const char *format, va_list ap) {
      if(linecache) {
        int retval = vsnprintf(prtcache, 1000, format, ap);
        if(retval >= 999) {
          throw std::runtime_error("ProcessImpl::vprintf(const char*,va_list): "
              "due to va_list restriction, cannot output 1000 or more characters at once "
              "in IODump mode.");
        }
        for(int i = 0; prtcache[i]; i++) {
          if(prtcache[i]=='\n') {
            fprintf(stderr, "%s<out>: %2d: %s\n", procname, line++, linecache->c_str());
            linecache->clear();
          } else {
            linecache->push_back(prtcache[i]);
          }
        }
        return fprintf(write_file, "%s", prtcache);
      } else {
        return vfprintf(write_file, format, ap);
      }
    }
    int flush() {
      return fflush(write_file);
    }
    void enableIODump() {
      if(!linecache) {
        linecache = new std::string();
        prtcache = new char[1000];
        reader.enableIODump();
      }
    }
  };
  class Process {
    Reader reader;
    ProcessImpl *process;
  public:
    void execute(const char *file, char *const argv[]) {
      if(process) throw std::domain_error("Process::execute(const char*,const char*): already executed.");
      if(!file) throw std::invalid_argument("Process::execute(const char*,const char*): file is NULL.");
      if(!argv) throw std::invalid_argument("Process::execute(const char*,const char*): argv is NULL.");
      FILE *read_file;
      FILE *write_file;
      int pid;
      int pipe_c2p[2], pipe_p2c[2];

      signal(SIGPIPE, SIG_IGN);
      if (pipe(pipe_c2p) < 0 || pipe(pipe_p2c) < 0) {
        throw std::runtime_error(std::string("error creating pipe: ")+strerror(errno));
      }
      if ((pid = fork()) < 0) {
        throw std::runtime_error(std::string("error forking process: ")+strerror(errno));
      }
      if (pid == 0) {
        close(pipe_p2c[1]); close(pipe_c2p[0]);
        dup2(pipe_p2c[0], 0); dup2(pipe_c2p[1], 1);
        close(pipe_p2c[0]); close(pipe_c2p[1]);
        execvp(file, argv);
        throw std::runtime_error(std::string("error executing process ")+file+": "+strerror(errno));
      }
      close(pipe_p2c[0]); close(pipe_c2p[1]);
      write_file = fdopen(pipe_p2c[1], "w");
      read_file = fdopen(pipe_c2p[0], "r");
      process = new ProcessImpl(file, reader,write_file,read_file,pid);
      process->ref_cnt++;
    }
    void execute(char *const argv[]) {
      execute(argv[0], argv);
    }
    void execute(char *const argv[], int extra_argc, va_list ap) {
      int argc = 0;
      for(; argv[argc]; argc++);
      char **argv2 = new char*[argc+extra_argc+1];
      for(int i = 0; i < argc; i++) {
        argv2[i] = argv[i];
      }
      for(int i = 0; i < extra_argc; i++) {
        argv2[argc+i] = va_arg(ap, char*);
      }
      argv2[argc+extra_argc] = NULL;
      execute(argv[0], argv2);
      delete[] argv2;
    }
    void execute(char *const argv[], int extra_argc, ...) {
      va_list ap;
      va_start(ap, extra_argc);
      execute(argv, extra_argc, ap);
      va_end(ap);
    }
    void execute(char *const argv[], char *earg1) {
      execute(argv, 1, earg1);
    }
    void execute(char *const argv[], char *earg1, char *earg2) {
      execute(argv, 2, earg1, earg2);
    }
    void execute(char *const argv[], char *earg1, char *earg2, char *earg3) {
      execute(argv, 3, earg1, earg2, earg3);
    }
    void execute(int argc, va_list ap) {
      char **argv = new char*[argc+1];
      for(int i = 0; i < argc; i++) {
        argv[i] = va_arg(ap, char*);
      }
      argv[argc] = NULL;
      execute(argv[0], argv);
      delete[] argv;
    }
    void execute(int argc, ...) {
      va_list ap;
      va_start(ap, argc);
      execute(argc, ap);
      va_end(ap);
    }
    void execute(char *arg1) {
      execute(1, arg1);
    }
    void execute(char *arg1, char *arg2) {
      execute(2, arg1, arg2);
    }
    void execute(char *arg1, char *arg2, char *arg3) {
      execute(3, arg1, arg2, arg3);
    }

    Process() : process(NULL) {}
    Process& operator=(const Process& that) {
      if(!--process->ref_cnt) {
        delete process;
      }
      process = that.process;
      process->ref_cnt++;
      return *this;
    }
    ~Process() {
      if(!--process->ref_cnt) {
        delete process;
      }
    }

    Process(const char *file, char *const argv[]) : process(NULL) {
      execute(file, argv);
    }
    Process(char *const argv[]) : process(NULL) {
      execute(argv);
    }
    Process(char *const argv[], int extra_argc, ...) : process(NULL) {
      va_list ap;
      va_start(ap, extra_argc);
      execute(argv, extra_argc, ap);
      va_end(ap);
    }
    Process(char *const argv[], char *earg1) : process(NULL) {
      execute(argv, 1, earg1);
    }
    Process(char *const argv[], char *earg1, char *earg2) : process(NULL){
      execute(argv, 2, earg1, earg2);
    }
    Process(char *const argv[], char *earg1, char *earg2, char *earg3) : process(NULL) {
      execute(argv, 3, earg1, earg2, earg3);
    }
    Process(int argc, ...) : process(NULL) {
      va_list ap;
      va_start(ap, argc);
      execute(argc, ap);
      va_end(ap);
    }
    Process(char *arg1) : process(NULL) {
      execute(1, arg1);
    }
    Process(char *arg1, char *arg2) : process(NULL) {
      execute(2, arg1, arg2);
    }
    Process(char *arg1, char *arg2, char *arg3) : process(NULL) {
      execute(3, arg1, arg2, arg3);
    }
    int scanf(const char *format, ...) ATTR_SCANF(2,3) {
      // reader.abortReading(); //TODO
      va_list ap;
      va_start(ap, format);
      int ret = process->vscanf(format, ap);
      va_end(ap);
      return ret;
    }
    int printf(const char *format, ...) ATTR_PRINTF(2,3) {
      va_list ap;
      va_start(ap, format);
      int ret = process->vprintf(format, ap);
      va_end(ap);
      return ret;
    }
    int flush() {
      return process->flush();
    }

    Reader& in() { return reader; }
    void closeProcess() {
      process->closeProcess();
    }

    void enableIODump() {
      process->enableIODump();
    }
  };
}

////
//// clean-up
////

#ifdef __GNUC__
#pragma GCC diagnostic pop
#undef ATTR_PRINTF
#undef ATTR_SCANF
#undef ATTR_NORETURN
#endif

#endif /* CHECKERLIB_H */
