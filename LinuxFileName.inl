// POSIX version of WindowsFileName.inl (used by the SDL build on macOS/Linux).
#include <sys/stat.h>

struct FILENAME // To avoid malloc and memory leaks
{
   std::string m_fName;

   FILE *Open(const char *name, const char *flags);
   FILE *Create(const char *name, const char *flags);
   i32 Rename(const char *oldname, const char *newname);
   i32 Unlink(const char *name);

private:
   std::string m_names[4];
   void createThreeNames(const char *filename);
   std::string createName(const char *folder, const char *file);
};

std::string FILENAME::createName(const char *folder, const char *file)
{
   if(!file)
      return {};
   if(strlen(file) == 0)
      return {};

   std::string result;
   if(!folder || !*folder)
      result = file;
   else
   {
      result = folder;
      if(result.back() != '/')
         result += '/';
      result += file;
   }
   return result;
}

void FILENAME::createThreeNames(const char *filename)
{
   for(auto &name : m_names)
      name.clear();

   if(filename[strspn(filename, " \t")] == '/')
   {
      m_names[0] = createName(nullptr, filename);
      return;
   }

   if(g_userRoot.empty())
   {
      if(g_folderName)
         m_names[0] = createName(g_folderName, filename);
      if(!g_folderParentName.empty())
         m_names[1] = createName(g_folderParentName.c_str(), filename);
      if(!g_root.empty())
         m_names[2] = createName(g_root.c_str(), filename);
      return;
   }

   // Inside the macOS .app the game data is read-only, so a relative
   // directory is taken to be inside the user's folder, and new files
   // (saves, config.txt) are created there.  The bundled copies of the
   // same folders are searched afterwards.
   bool relative = g_folderName && g_folderName[0] != '/';
   if(g_folderName)
      m_names[0] = createName(relative ? createName(g_userRoot.c_str(), g_folderName).c_str() : g_folderName, filename);
   if(relative && !g_folderParentName.empty())
      m_names[1] = createName(createName(g_userRoot.c_str(), g_folderParentName.c_str()).c_str(), filename);
   else if(relative || !g_folderName)
      m_names[1] = createName(g_userRoot.c_str(), filename);
   else if(!g_folderParentName.empty())
      m_names[1] = createName(g_folderParentName.c_str(), filename);
   if(relative)
      m_names[2] = createName(createName(g_root.c_str(), g_folderName).c_str(), filename);
   m_names[3] = createName(g_root.c_str(), filename);
}

FILE *FILENAME::Open(const char *name, const char *flags)
{
   createThreeNames(name);
   for(unsigned i = 0; i < 4; i++)
   {
      if(m_names[i].empty())
         continue;
      FILE *result = UI_fopen(m_names[i].c_str(), flags);
      if(result)
      {
         if(TimerTraceActive)
            fprintf(GETFILE(TraceFile), "Opened %s\n", m_names[i].c_str());
         m_fName = m_names[i];
         return result;
      }
   }
   return nullptr;
}

FILE *FILENAME::Create(const char *name, const char *flags)
{
   createThreeNames(name);
   for(auto &name : m_names)
   {
      if(name.empty())
         continue;
      if(!g_userRoot.empty())
      {
         // The game folder inside the user's folder may not exist yet.
         std::string::size_type slash = name.rfind('/');
         if(slash != std::string::npos && slash > 0)
            mkdir(name.substr(0, slash).c_str(), 0755);
      }
      return UI_fopen(name.c_str(), flags);
   };
   return nullptr;
}

i32 FILENAME::Rename(const char *oldname, const char *newname)
{
   FILENAME newfile;
   createThreeNames(oldname);
   newfile.createThreeNames(newname);
   for(unsigned i = 0; i < 4; i++)
   {
      if(m_names[i].empty())
         continue;
      return rename(m_names[i].c_str(), newfile.m_names[i].c_str());
   };
   return -1;
}

i32 FILENAME::Unlink(const char *name)
{
   createThreeNames(name);
   for(auto &name : m_names)
   {
      if(name.empty())
         continue;
      return UI_DeleteFile(name.c_str());
   };
   return 0;
}

void UnlinkFile(const char *name)
{
   FILENAME file;
   file.Unlink(name);
}

const char *GETFILENAME(i32 f);

ui64 MODIFIEDTIME(i32 file)
{
   const char *fn = GETFILENAME(file);
   if(!fn)
      return 0;

   struct stat info;
   if(stat(fn, &info) != 0)
      return 0;
   return ui64(info.st_mtime);
}
