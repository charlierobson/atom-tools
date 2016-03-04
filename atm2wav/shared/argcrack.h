#ifndef __argcrack_h
#define __argcrack_h

class argcrack
{
public:
	argcrack(int argc, char **argv) :
	  m_argc(argc),
		m_argv(argv)
	  {
	  }

	  bool getint(const char* pname, int& target)
	  {
		  try
		  {
			  for(int i = 1; i < m_argc; ++i)
			  {
				  if (_strnicmp(pname,m_argv[i],strlen(pname)) == 0)
				  {
					  const char* val = &m_argv[i][strlen(pname)+1];

					  int base = 0;
					  if (*val == '%')
					  {
						  base = 2;
						  ++val;
					  }

					  target = strtol(val,NULL,base);

					  return true;
				  }
			  }
		  }
		  catch(...)
		  {
		  }

		  return false;
	  }


	  bool getstring(const char* pname, std::string& target)
	  {
		  try
		  {
			  for(int i = 1; i < m_argc; ++i)
			  {
				  if (_strnicmp(pname,m_argv[i],strlen(pname)) == 0)
				  {
					  target = &m_argv[i][strlen(pname)+1];
					  return true;
				  }
			  }
		  }
		  catch(...)
		  {
		  }

		  return false;
	  }


	  bool ispresent(const char* pname)
	  {
		  try
		  {
			  for(int i = 1; i < m_argc; ++i)
			  {
				  if (_strnicmp(pname,m_argv[i],strlen(pname)) == 0)
				  {
					  return true;
				  }
			  }
		  }
		  catch(...)
		  {
		  }

		  return false;
	  }

private:
	int m_argc;
	char** m_argv;
};

#endif
