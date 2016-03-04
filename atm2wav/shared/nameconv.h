#ifndef __nameconv_h
#define __nameconv_h

#include <string>

// Atom names.
// Upper-case, no spaces, underscores. Max 11 chars.

std::string pc_to_atom(const char* pc_in)
{
   std::string mangled_name_out;

   std::string pc_name_in(pc_in);

   // 'a very long file.ext' -> 'AVERYLONGFI'
   // 'a.b.com' -> 'A.B'
   // 'a_b.com' -> 'AB'
   // 'a b.com' -> 'AB'

   size_t pos = pc_name_in.find_last_of(".");
   if (pos != std::string::npos)
   {
      pc_name_in.erase(pos);
   }

   std::string erase_these(" _");
   for (size_t i = 0; mangled_name_out.size() < 11 && i < pc_name_in.size(); ++i)
   {
      if (erase_these.find_first_of(pc_name_in[i]) == std::string::npos)
      {
         mangled_name_out += toupper(pc_name_in[i]);
      }
   }

   return mangled_name_out;
}


#endif
