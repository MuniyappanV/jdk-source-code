#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)arguments.cpp	1.264 04/06/08 15:50:08 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_arguments.cpp.incl"

#define DEFAULT_VENDOR_URL_BUG "http://java.sun.com/webapps/bugreport/crash.jsp"

char*  Arguments::_jvm_flags                    = NULL;
char*  Arguments::_jvm_args                     = NULL;
char*  Arguments::_java_command                 = NULL;
SystemProperty* Arguments::_system_properties   = NULL;
const char*  Arguments::_gc_log_filename        = NULL;
bool   Arguments::_has_profile                  = false;
bool   Arguments::_has_alloc_profile            = false;
uintx  Arguments::_initial_heap_size            = 0;
Arguments::Mode Arguments::_mode                = _mixed;
bool   Arguments::_java_compiler                = false;
const char*  Arguments::_java_vendor_url_bug    = DEFAULT_VENDOR_URL_BUG;

// These parameters are reset in method parse_vm_init_args(JavaVMInitArgs*)
bool   Arguments::_AlwaysCompileLoopMethods     = AlwaysCompileLoopMethods;
bool   Arguments::_UseOnStackReplacement        = UseOnStackReplacement;
bool   Arguments::_BackgroundCompilation        = BackgroundCompilation;
bool   Arguments::_ClipInlining                 = ClipInlining;
intx   Arguments::_Tier2CompileThreshold        = Tier2CompileThreshold;

short  Arguments::CompileOnlyClassesNum	        = 0;
short  Arguments::CompileOnlyClassesMax	        = 0;
char** Arguments::CompileOnlyClasses		= NULL;
bool*  Arguments::CompileOnlyAllMethods	        = NULL;

short  Arguments::CompileOnlyMethodsNum	        = 0;
short  Arguments::CompileOnlyMethodsMax	        = 0;
char** Arguments::CompileOnlyMethods		= NULL;
bool*  Arguments::CompileOnlyAllClasses	        = NULL;

bool   Arguments::CheckCompileOnly              = false;

char*  Arguments::SharedArchivePath             = NULL;

AgentLibraryList Arguments::_libraryList;
AgentLibraryList Arguments::_agentList;

abort_hook_t     Arguments::_abort_hook         = NULL;
exit_hook_t      Arguments::_exit_hook          = NULL;
vfprintf_hook_t  Arguments::_vfprintf_hook      = NULL;


SystemProperty *Arguments::_java_ext_dirs = NULL;
SystemProperty *Arguments::_java_endorsed_dirs = NULL;
SystemProperty *Arguments::_sun_boot_library_path = NULL;
SystemProperty *Arguments::_java_library_path = NULL;
SystemProperty *Arguments::_java_home = NULL;
SystemProperty *Arguments::_java_class_path = NULL;
SystemProperty *Arguments::_sun_boot_class_path = NULL;

// Initialize system properties key and value.
void Arguments::init_system_properties() {
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.specification.version", "1.0", false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.specification.name",
                                                                 "Java Virtual Machine Specification",  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.specification.vendor",
                                                                 "Sun Microsystems Inc.",  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.version", VM_Version::vm_release(),  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.name", VM_Version::vm_name(),  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.vendor", VM_Version::vm_vendor(),  false));
  PropertyList_add(&_system_properties, new SystemProperty("java.vm.info", VM_Version::vm_info_string(),  false));

  // following are JVMTI agent writeable properties.
  // Properties values are set to NULL and they are
  // os specific they are initialized in os::init_system_properties_values().
  _java_ext_dirs = new SystemProperty("java.ext.dirs", NULL,  true);
  _java_endorsed_dirs = new SystemProperty("java.endorsed.dirs", NULL,  true);
  _sun_boot_library_path = new SystemProperty("sun.boot.library.path", NULL,  true);
  _java_library_path = new SystemProperty("java.library.path", NULL,  true);
  _java_home =  new SystemProperty("java.home", NULL,  true);
  _sun_boot_class_path = new SystemProperty("sun.boot.class.path", NULL,  true);

  _java_class_path = new SystemProperty("java.class.path", "",  true);

  // Add to System Property list.
  PropertyList_add(&_system_properties, _java_ext_dirs);
  PropertyList_add(&_system_properties, _java_endorsed_dirs);
  PropertyList_add(&_system_properties, _sun_boot_library_path);
  PropertyList_add(&_system_properties, _java_library_path);
  PropertyList_add(&_system_properties, _java_home);
  PropertyList_add(&_system_properties, _java_class_path);
  PropertyList_add(&_system_properties, _sun_boot_class_path);

  // Set OS specific system properties values
  os::init_system_properties_values();
}

// Constructs the system class path (aka boot class path) from the following
// components, in order:
// 
//     prefix		// from -Xbootclasspath/p:...
//     endorsed		// the expansion of -Djava.endorsed.dirs=...
//     base		// from os::get_system_properties() or -Xbootclasspath=
//     suffix		// from -Xbootclasspath/a:...
// 
// java.endorsed.dirs is a list of directories; any jar or zip files in the
// directories are added to the sysclasspath just before the base.
//
// This could be AllStatic, but it isn't needed after argument processing is
// complete.
class SysClassPath: public StackObj {
public:
  SysClassPath(const char* base);
  ~SysClassPath();

  inline void set_base(const char* base);
  inline void add_prefix(const char* prefix);
  inline void add_suffix(const char* suffix);
  inline void reset_path(const char* base);

  // Expand the jar/zip files in each directory listed by the java.endorsed.dirs
  // property.  Must be called after all command-line arguments have been
  // processed (in particular, -Djava.endorsed.dirs=...) and before calling
  // combined_path().
  void expand_endorsed();

  inline const char* get_base()     const { return _items[_scp_base]; }
  inline const char* get_prefix()   const { return _items[_scp_prefix]; }
  inline const char* get_suffix()   const { return _items[_scp_suffix]; }
  inline const char* get_endorsed() const { return _items[_scp_endorsed]; }

  // Combine all the components into a single c-heap-allocated string; caller
  // must free the string if/when no longer needed.
  char* combined_path();

private:
  // Utility routines.
  static char* add_to_path(const char* path, const char* str, bool prepend);
  static char* add_jars_to_path(char* path, const char* directory);

  inline void reset_item_at(int index);

  // Array indices for the items that make up the sysclasspath.  All except the
  // base are allocated in the C heap and freed by this class.
  enum {
    _scp_prefix,	// from -Xbootclasspath/p:...
    _scp_endorsed,	// the expansion of -Djava.endorsed.dirs=...
    _scp_base,		// the default sysclasspath
    _scp_suffix,	// from -Xbootclasspath/a:...
    _scp_nitems		// the number of items, must be last.
  };

  const char* _items[_scp_nitems];
  DEBUG_ONLY(bool _expansion_done;)
};

SysClassPath::SysClassPath(const char* base) {
  memset(_items, 0, sizeof(_items));
  _items[_scp_base] = base;
  DEBUG_ONLY(_expansion_done = false;)
}

SysClassPath::~SysClassPath() {
  // Free everything except the base.
  for (int i = 0; i < _scp_nitems; ++i) {
    if (i != _scp_base) reset_item_at(i);
  }
  DEBUG_ONLY(_expansion_done = false;)
}

inline void SysClassPath::set_base(const char* base) {
  _items[_scp_base] = base;
}

inline void SysClassPath::add_prefix(const char* prefix) {
  _items[_scp_prefix] = add_to_path(_items[_scp_prefix], prefix, true);
}

inline void SysClassPath::add_suffix(const char* suffix) {
  _items[_scp_suffix] = add_to_path(_items[_scp_suffix], suffix, false);
}

inline void SysClassPath::reset_item_at(int index) {
  assert(index < _scp_nitems && index != _scp_base, "just checking");
  if (_items[index] != NULL) {
    FREE_C_HEAP_ARRAY(char, _items[index]);
    _items[index] = NULL;
  }
}

inline void SysClassPath::reset_path(const char* base) {
  // Clear the prefix and suffix.
  reset_item_at(_scp_prefix);
  reset_item_at(_scp_suffix);
  set_base(base);
}

//------------------------------------------------------------------------------

void SysClassPath::expand_endorsed() {
  assert(_items[_scp_endorsed] == NULL, "can only be called once.");

  const char* path = Arguments::get_property("java.endorsed.dirs");
  if (path == NULL) {
    path = Arguments::get_endorsed_dir();
    assert(path != NULL, "no default for java.endorsed.dirs");
  }

  char* expanded_path = NULL;
  const char separator = *os::path_separator();
  const char* const end = path + strlen(path);
  while (path < end) {
    const char* tmp_end = strchr(path, separator);
    if (tmp_end == NULL) {
      expanded_path = add_jars_to_path(expanded_path, path);
      path = end;
    } else {
      char* dirpath = NEW_C_HEAP_ARRAY(char, tmp_end - path + 1);
      memcpy(dirpath, path, tmp_end - path);
      dirpath[tmp_end - path] = '\0';
      expanded_path = add_jars_to_path(expanded_path, dirpath);
      FREE_C_HEAP_ARRAY(char, dirpath);
      path = tmp_end + 1;
    }
  }
  _items[_scp_endorsed] = expanded_path;
  DEBUG_ONLY(_expansion_done = true;)
}

// Combine the bootclasspath elements, some of which may be null, into a single
// c-heap-allocated string.
char* SysClassPath::combined_path() {
  assert(_items[_scp_base] != NULL, "empty default sysclasspath");
  assert(_expansion_done, "must call expand_endorsed() first.");

  size_t lengths[_scp_nitems];
  size_t total_len = 0;

  const char separator = *os::path_separator();

  // Get the lengths.
  int i;
  for (i = 0; i < _scp_nitems; ++i) {
    if (_items[i] != NULL) {
      lengths[i] = strlen(_items[i]);
      // Include space for the separator char (or a NULL for the last item).
      total_len += lengths[i] + 1;
    }
  }
  assert(total_len > 0, "empty sysclasspath not allowed");

  // Copy the _items to a single string.
  char* cp = NEW_C_HEAP_ARRAY(char, total_len);
  char* cp_tmp = cp;
  for (i = 0; i < _scp_nitems; ++i) {
    if (_items[i] != NULL) {
      memcpy(cp_tmp, _items[i], lengths[i]);
      cp_tmp += lengths[i];
      *cp_tmp++ = separator;
    }
  }
  *--cp_tmp = '\0';	// Replace the extra separator.
  return cp;
}

// Note:  path must be c-heap-allocated (or NULL); it is freed if non-null.
char*
SysClassPath::add_to_path(const char* path, const char* str, bool prepend) {
  char *cp;

  assert(str != NULL, "just checking");
  if (path == NULL) {
    size_t len = strlen(str) + 1;
    cp = NEW_C_HEAP_ARRAY(char, len);
    memcpy(cp, str, len);			// copy the trailing null
  } else {
    const char separator = *os::path_separator();
    size_t old_len = strlen(path);
    size_t str_len = strlen(str);
    size_t len = old_len + str_len + 2;

    if (prepend) {
      cp = NEW_C_HEAP_ARRAY(char, len);
      char* cp_tmp = cp;
      memcpy(cp_tmp, str, str_len);
      cp_tmp += str_len;
      *cp_tmp = separator;
      memcpy(++cp_tmp, path, old_len + 1);	// copy the trailing null
      FREE_C_HEAP_ARRAY(char, path);
    } else {
      cp = REALLOC_C_HEAP_ARRAY(char, path, len);
      char* cp_tmp = cp + old_len;
      *cp_tmp = separator;
      memcpy(++cp_tmp, str, str_len + 1);	// copy the trailing null
    }
  }
  return cp;
}

// Scan the directory and append any jar or zip files found to path.
// Note:  path must be c-heap-allocated (or NULL); it is freed if non-null.
char* SysClassPath::add_jars_to_path(char* path, const char* directory) {
  DIR* dir = os::opendir(directory);
  if (dir == NULL) return path;
  
  char dir_sep[2] = { '\0', '\0' };
  size_t directory_len = strlen(directory);
  const char fileSep = *os::file_separator();
  if (directory[directory_len - 1] != fileSep) dir_sep[0] = fileSep;
    
  /* Scan the directory for jars/zips, appending them to path. */ 
  struct dirent *entry;
  char *dbuf = NEW_C_HEAP_ARRAY(char, os::readdir_buf_size(directory));
  while ((entry = os::readdir(dir, (dirent *) dbuf)) != NULL) {
    const char* name = entry->d_name;
    const char* ext = name + strlen(name) - 4;
    bool isJarOrZip = ext > name &&
      (os::file_name_strcmp(ext, ".jar") == 0 ||
       os::file_name_strcmp(ext, ".zip") == 0);
    if (isJarOrZip) {
      char* jarpath = NEW_C_HEAP_ARRAY(char, directory_len + 2 + strlen(name));
      sprintf(jarpath, "%s%s%s", directory, dir_sep, name);
      path = add_to_path(path, jarpath, false);
      FREE_C_HEAP_ARRAY(char, jarpath);
    }
  }
  FREE_C_HEAP_ARRAY(char, dbuf);
  os::closedir(dir);
  return path;
}

// Parses a memory size specification string.
static bool atomll(const char *s, jlong* result) {
  jlong n = 0;
  int args_read = sscanf(s, os::jlong_format_specifier(), &n);
  if (args_read != 1) {
    return false;
  }
  while (*s != '\0' && isdigit(*s)) {
    s++;
  }
  // 4705540: illegal if more characters are found after the first non-digit
  if (strlen(s) > 1) {
    return false;
  }
  switch (*s) {
    case 'T': case 't':
      *result = n * G * K;
      return true;
    case 'G': case 'g':
      *result = n * G;
      return true;
    case 'M': case 'm':
      *result = n * M;
      return true;
    case 'K': case 'k':
      *result = n * K;
      return true;
    case '\0':
      *result = n;
      return true;
    default:
      return false;
  }
}

Arguments::ArgsRange Arguments::check_memory_size(jlong size, jlong min_size) {
  if (size < min_size) return arg_too_small;
  // Check that size will fit in a size_t (only relevant on 32-bit)
  if ((julong) size > max_uintx) return arg_too_big;
  return arg_in_range;
}

// Describe an argument out of range error 
void Arguments::describe_range_error(ArgsRange errcode) {
  switch(errcode) {
  case arg_too_big:
    jio_fprintf(defaultStream::error_stream(),
                "The specified size exceeds the maximum "
		"representable size.\n");
    break;
  case arg_too_small:
  case arg_unreadable:
  case arg_in_range:
    // do nothing for now
    break;
  default:
    ShouldNotReachHere();
  }
}

static bool set_bool_flag(char* name, bool value) {
  return CommandLineFlags::boolAtPut(name, &value);
}


static bool set_numeric_flag(char* name, char* value) {
  jlong v;
  if (!atomll(value, &v)) {
    return false;
  }
  intx intx_v = (intx) v;
  if (CommandLineFlags::intxAtPut(name, &intx_v)) {
    return true;
  }
  uintx uintx_v = (uintx) v;
  if (CommandLineFlags::uintxAtPut(name, &uintx_v)) {
    return true;
  }
  return false;
}


static bool set_string_flag(char* name, const char* value) {
  if (!CommandLineFlags::ccstrAtPut(name, &value))  return false;
  // Contract:  CommandLineFlags always returns a pointer that needs freeing.
  FREE_C_HEAP_ARRAY(char, value);
  return true;
}

static bool append_to_string_flag(char* name, const char* new_value) {
  const char* old_value = "";
  if (!CommandLineFlags::ccstrAt(name, &old_value))  return false;
  size_t old_len = strlen(old_value);
  size_t new_len = strlen(new_value);
  const char* value;
  char* free_this_too = NULL;
  if (old_len == 0) {
    value = new_value;
  } else if (new_len == 0) {
    value = old_value;
  } else {
    char* buf = NEW_C_HEAP_ARRAY(char, old_len + 1 + new_len + 1);
    // each new setting adds another LINE to the switch:
    sprintf(buf, "%s\n%s", old_value, new_value);
    value = buf;
    free_this_too = buf;
  }
  (void) CommandLineFlags::ccstrAtPut(name, &value);
  // CommandLineFlags always returns a pointer that needs freeing.
  FREE_C_HEAP_ARRAY(char, value);
  if (free_this_too != NULL) {
    // CommandLineFlags made its own copy, so I must delete my own temp. buffer.
    FREE_C_HEAP_ARRAY(char, free_this_too);
  }
  return true;
}


void Arguments::parseOnlyLine (const char* line, 
  short* classesNum, short* classesMax, char*** classes, bool** allMethods,
  short* methodsNum, short* methodsMax, char*** methods, bool** allClasses) {

  int i;
  char name[1024];
  bool className = true;	// current string is class name.
  bool addedClass = false;	
  bool addedMethod = false;

  while (*line != '\0') {
    for (i = 0 ; i < 1024 && *line != '\0' && *line != '.' && *line != ',' && !isspace(*line); line++, i++)
      name[i] = *line;

    if (i > 0) {
      char* newName = NEW_C_HEAP_ARRAY( char, i + 1);
      if (newName == NULL)
        return;
      strncpy( newName, name, i);
      newName[i] = '\0';

      if (className) {
        addedClass = true;
        if (*classesNum == *classesMax) {
          *classesMax += 5;
          *classes = REALLOC_C_HEAP_ARRAY( char*, *classes, *classesMax);
          *allMethods = REALLOC_C_HEAP_ARRAY( bool, *allMethods, *classesMax);
          if (*classes == NULL || *allMethods == NULL)
            return;
        }
        (*classes)[*classesNum] = newName;
        (*allMethods)[*classesNum] = false;
        (*classesNum)++;
      }
      else {
        addedMethod = true;
        if (*methodsNum == *methodsMax) {
          *methodsMax += 5;

          *methods = REALLOC_C_HEAP_ARRAY( char*, *methods, *methodsMax);
          *allClasses = REALLOC_C_HEAP_ARRAY( bool, *allClasses, *methodsMax);
          if (*methods == NULL || *allClasses == NULL)
            return;
        }
        (*methods)[*methodsNum] = newName;
        (*allClasses)[*methodsNum] = false;
        (*methodsNum)++;
      }
    }

    if (*line == '.')
      className = false;

    if (*line == ',' || isspace(*line) || *line == '\0') {
      if (addedClass && !addedMethod) {
        (*allMethods)[*classesNum - 1] = true;
      }
      if (addedMethod && !addedClass) {
        (*allClasses)[*methodsNum - 1] = true;
      }
      className = true;
      addedClass = false;
      addedMethod = false;
    }

    line = *line == '\0' ? line : line + 1;
  }
}

bool Arguments::parse_argument(const char* arg) {

  // range of acceptable characters spelled out for portability reasons
  #define NAME_RANGE  "[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_]"
  #define BUFLEN 255
  char name[BUFLEN+1];
  char dummy;

  if (sscanf(arg, "-%" XSTR(BUFLEN) NAME_RANGE "%c", name, &dummy) == 1) {
    return set_bool_flag(name, false);
  }
  if (sscanf(arg, "+%" XSTR(BUFLEN) NAME_RANGE "%c", name, &dummy) == 1) {
    return set_bool_flag(name, true);
  }

  char punct;
  if (sscanf(arg, "%" XSTR(BUFLEN) NAME_RANGE "%c", name, &punct) == 2 && punct == '=') {
    const char* value = strchr(arg, '=') + 1;
    // Note that normal -XX:Foo=WWW accumulates.
    bool success = append_to_string_flag(name, value);

    if (success && strcmp(name, "CompileOnly") == 0) {
      // Record the classes and methods that should always be compiled.
      // %%% This stuff belongs in the CompilerOracle, not here.
      parseOnlyLine(value,
                    &CompileOnlyClassesNum, &CompileOnlyClassesMax, &CompileOnlyClasses, &CompileOnlyAllMethods,
                    &CompileOnlyMethodsNum, &CompileOnlyMethodsMax, &CompileOnlyMethods, &CompileOnlyAllClasses);
      if (CompileOnlyClassesNum > 0 || CompileOnlyMethodsNum > 0) {
        CheckCompileOnly = true;
      }
    }

    if (success)  return success;
  }

  if (sscanf(arg, "%" XSTR(BUFLEN) NAME_RANGE ":%c", name, &punct) == 2 && punct == '=') {
    const char* value = strchr(arg, '=') + 1;
    // -XX:Foo:=xxx will reset the string flag to the given value.
    return set_string_flag(name, value);
  }

  #define VALUE_RANGE "[-kmgtKMGT0123456789]"
  char value[BUFLEN + 1];
  if (sscanf(arg, "%" XSTR(BUFLEN) NAME_RANGE "=" "%" XSTR(BUFLEN) VALUE_RANGE "%c", name, value, &dummy) == 2) {
    return set_numeric_flag(name, value);
  }

  return false;
}


void Arguments::build_string(char** bldstr, const char* arg) {
  
  assert(bldstr != NULL, "illegal argument");

  if (arg == NULL) {
    return;
  }

  if (*bldstr == NULL) {
     // allocate enough space for the new build string to hold
     // the given argument string and a null terminator
     *bldstr = NEW_C_HEAP_ARRAY(char, strlen(arg) + 1);
     **bldstr = '\0';
  }
  else {
     // allocate enough space for the new build string to hold
     // the existing build string, the argument string, a space
     // to separate the existing string and the new argument,
     // and the null terminator
     size_t new_length = strlen(*bldstr) + strlen(arg) + 2;
     *bldstr = REALLOC_C_HEAP_ARRAY(char, *bldstr, new_length);
  }

  if (strlen(*bldstr) > 0)
    // append a space character onto the existing build string
    strcat(*bldstr, " ");

  // append the argument string onto the build string
  strcat(*bldstr, arg);
}

void Arguments::build_jvm_args(const char* arg) {
  build_string(&_jvm_args, arg);
}

void Arguments::build_jvm_flags(const char* arg) {
  build_string(&_jvm_flags, arg);
}

void Arguments::print_on(outputStream* st) {
  st->print_cr("VM Arguments:");
  if (jvm_flags()) st->print_cr("jvm_flags: %s", jvm_flags());
  if (jvm_args())  st->print_cr("jvm_args: %s", jvm_args());
  st->print_cr("java_command: %s", java_command() ? java_command() : "<unknown>");
}

bool Arguments::process_argument(const char* arg, bool ignore_unrecognized) {

  if (parse_argument(arg)) {
    if (PrintVMOptions) {
      jio_fprintf(defaultStream::output_stream(), "VM option '%s'\n", arg);
    }
  } else {
    if (!ignore_unrecognized) {
      jio_fprintf(defaultStream::error_stream(),
		  "Unrecognized VM option '%s'\n", arg);
      // allow for commandline "commenting out" options like -XX:#+Verbose
      if (strlen(arg) == 0 || arg[0] != '#') {
        return false;
      }
    }
  }
  return true;
}


bool Arguments::process_settings_file(const char* file_name, bool should_exist, bool ignore_unrecognized) {
  FILE* stream = fopen(file_name, "rb");
  if (stream == NULL) {
    if (should_exist) {
      jio_fprintf(defaultStream::error_stream(),
		  "Could not open settings file %s\n", file_name);
      return false;
    } else {
      return true;
    }
  }

  char token[1024];
  int  pos = 0;

  bool in_white_space = true;
  bool in_comment     = false;
  bool in_quote       = false;
  char quote_c        = 0;
  bool result         = true;

  int c = getc(stream);
  while(c != EOF) {
    if (in_white_space) {
      if (in_comment) {
	if (c == '\n') in_comment = false;
      } else {
        if (c == '#') in_comment = true;
        else if (!isspace(c)) {
          in_white_space = false;
	  token[pos++] = c;
        }
      }
    } else {
      if (c == '\n' || (!in_quote && isspace(c))) {
	// token ends at newline, or at unquoted whitespace
	// this allows a way to include spaces in string-valued options
        token[pos] = '\0';
        result &= process_argument(token, ignore_unrecognized);
        build_jvm_flags(token);
	pos = 0;
	in_white_space = true;
	in_quote = false;
      } else if (!in_quote && (c == '\'' || c == '"')) {
	in_quote = true;
	quote_c = c;
      } else if (in_quote && (c == quote_c)) {
	in_quote = false;
      } else {
        token[pos++] = c;
      }
    }
    c = getc(stream);
  }
  if (pos > 0) {
    token[pos] = '\0';
    result &= process_argument(token, ignore_unrecognized);
    build_jvm_flags(token);
  }
  fclose(stream);
  return result;
}

//=============================================================================================================
// Parsing of properties (-D) 

const char* Arguments::get_property(const char* key) {
  return PropertyList_get_value(system_properties(), key);
}

bool Arguments::add_property(const char* prop) {
  const char* eq = strchr(prop, '=');
  char* key;
  // ns must be static--its address may be stored in a SystemProperty object.
  const static char ns[1] = {0};
  char* value = (char *)ns;

  size_t key_len = (eq == NULL) ? strlen(prop) : (eq - prop);
  key = AllocateHeap(key_len + 1, "add_property");
  strncpy(key, prop, key_len);
  key[key_len] = '\0';

  if (eq != NULL) {
    size_t value_len = strlen(prop) - key_len - 1;
    value = AllocateHeap(value_len + 1, "add_property");
    strncpy(value, &prop[key_len + 1], value_len + 1);    
  }

  if (strcmp(key, "java.compiler") == 0) {
    process_java_compiler_argument(value);
    FreeHeap(key);
    if (eq != NULL) {
      FreeHeap(value);
    }
    return true;
  }
  else if (strcmp(key, "sun.java.command") == 0) {

    _java_command = value;

    // don't add this property to the properties exposed to the java application
    FreeHeap(key);
    return true;
  }
  else if (strcmp(key, "java.vendor.url.bug") == 0) {
    // save it in _java_vendor_url_bug, so JVM fatal error handler can access
    // its value without going through the property list or making a Java call.
    _java_vendor_url_bug = value;
  }

  // Create new property and add at the end of the list
  PropertyList_unique_add(&_system_properties, key, value);
  return true;
}

//===========================================================================================================
// Setting int/mixed/comp mode flags 

void Arguments::set_mode_flags(Mode mode) {
  // Set up default values for all flags.
  // If you add a flag to any of the branches below,
  // add a default value for it here.
  set_java_compiler(false);
  _mode                      = mode;
  UseInterpreter             = true;
  UseCompiler                = true;
  UseLoopCounter             = true;

  // Default values may be platform/compiler dependent -
  // use the saved values
  ClipInlining               = Arguments::_ClipInlining;
  AlwaysCompileLoopMethods   = Arguments::_AlwaysCompileLoopMethods;
  UseOnStackReplacement      = Arguments::_UseOnStackReplacement;
  BackgroundCompilation      = Arguments::_BackgroundCompilation;
  Tier2CompileThreshold      = Arguments::_Tier2CompileThreshold;

  // Change from defaults based on mode
  switch (mode) {
  default:
    ShouldNotReachHere();
    break;
  case _int:
    UseCompiler              = false;
    UseLoopCounter           = false;
    AlwaysCompileLoopMethods = false;
    UseOnStackReplacement    = false;
    break;
  case _mixed:
    // same as default
    break;
  case _comp:
    UseInterpreter           = false;
    BackgroundCompilation    = false;
    ClipInlining             = false;
    Tier2CompileThreshold    = 1000;
    break;
  }
}


// Conflict: required to use shared spaces (-Xshare:on), but
// incompatible command line options were chosen.

static void no_shared_spaces() {
  if (RequireSharedSpaces) {
    jio_fprintf(defaultStream::error_stream(),
      "Class data sharing is inconsistent with other specified options.\n");
    vm_exit_during_initialization("Unable to use shared archive.", NULL);
  } else {
    FLAG_SET_DEFAULT(UseSharedSpaces, false);
  }
}


// If the user has chosen ParallelGCThreads > 0, we set UseParNewGC
// if it's not explictly set or unset. If the user has chosen
// UseParNewGC and not explicitly set ParallelGCThreads we 
// set it, unless this is a single cpu machine.
void Arguments::set_parnew_gc_flags() {
  assert(!UseSerialGC && !UseParallelGC && !UseTrainGC, 
         "control point invariant");

  if (FLAG_IS_DEFAULT(UseParNewGC) && ParallelGCThreads > 1) {
    FLAG_SET_DEFAULT(UseParNewGC, true);
  } else if (UseParNewGC && ParallelGCThreads == 0) {
    FLAG_SET_DEFAULT(ParallelGCThreads, nof_parallel_gc_threads());
    if (FLAG_IS_DEFAULT(ParallelGCThreads) && ParallelGCThreads == 1) {
      FLAG_SET_DEFAULT(UseParNewGC, false);
    }
  }
  if (!UseParNewGC) {
    FLAG_SET_DEFAULT(ParallelGCThreads, 0);
  } else {
    no_shared_spaces();
  }
}

// CAUTION: this code is currently shared by UseParallelGC, UseParNewGC and
// UseconcMarkSweepGC. Further tuning of individual collectors might 
// dictate refinement on a per-collector basis.
int Arguments::nof_parallel_gc_threads() {
  if (FLAG_IS_DEFAULT(ParallelGCThreads)) {
    // For very large machines, there are diminishing returns
    // for large numbers of worker threads.  Instead of
    // hogging the whole system, use 5/8ths of a worker for every
    // processor after the first 8.  For example, on a 72 cpu
    // machine use 8 + (72 - 8) * (5/8) == 48 worker threads.
    // This is just a start and needs further tuning and study in
    // Tiger.
    int ncpus = os::active_processor_count();
    return (ncpus <= 8) ? ncpus : 3 + ((ncpus * 5) / 8);
  } else {
    return ParallelGCThreads;
  }
}

// Adjust some sizes to suit CMS and/or ParNew needs; these work well on
// sparc/solaris for certain applications, but would gain from
// further optimization and tuning efforts, and would almost
// certainly gain from analysis of platform and environment.
void Arguments::set_cms_and_parnew_gc_flags() {
  if (UseSerialGC || UseParallelGC || UseTrainGC) {
    return;
  }

  // If we are using CMS, we prefer to UseParNewGC,
  // unless explicitly forbidden.
  if (UseConcMarkSweepGC && !UseParNewGC && FLAG_IS_DEFAULT(UseParNewGC)) {
    FLAG_SET_DEFAULT(UseParNewGC, true);
  }

  // In either case, adjust ParallelGCThreads and/or UseParNewGC
  // as needed.
  set_parnew_gc_flags();

  if (!UseConcMarkSweepGC) {
    return;
  }

  // Now make adjustments for CMS

  // Preferred young gen size for "short" pauses
  const uintx parallel_gc_threads = 
    (ParallelGCThreads == 0 ? 1 : ParallelGCThreads);
  const size_t preferred_max_new_size_unaligned =
    ScaleForWordSize(4 * M) * parallel_gc_threads;
  const size_t preferred_max_new_size = 
    align_size_down(preferred_max_new_size_unaligned, os::vm_page_size());

  // Unless explicitly requested otherwise, size young gen
  // for "short" pauses ~ 4M*ParallelGCThreads
  if (FLAG_IS_DEFAULT(MaxNewSize)) {  // MaxNewSize not set at command-line
    if (!FLAG_IS_DEFAULT(NewSize)) {   // NewSize explicitly set at command-line
      FLAG_SET_DEFAULT(MaxNewSize, MAX2(NewSize, preferred_max_new_size));
    } else {
      FLAG_SET_DEFAULT(MaxNewSize, preferred_max_new_size);
    }
  }
  size_t min_new  = align_size_up(ScaleForWordSize(4*M), os::vm_page_size());
  size_t max_heap = align_size_down(MaxHeapSize,CardTableRS::ct_max_alignment_constraint());
                    // MaxHeapSize is aligned down in collectorPolicy

  if (max_heap > min_new) {
    // Unless explicitly requested otherwise, make young gen
    // at least min_new, and at most preferred_max_new_size.
    if (FLAG_IS_DEFAULT(NewSize)) {
      FLAG_SET_DEFAULT(NewSize, MAX2(NewSize, min_new));
      FLAG_SET_DEFAULT(NewSize, MIN2(preferred_max_new_size, NewSize));
    }
    // Unless explicitly requested otherwise, size old gen
    // so that it's at least 3X of NewSize to begin with;
    // later NewRatio will decide how it grows; see below.
    if (FLAG_IS_DEFAULT(OldSize)) {
      if (max_heap > NewSize) {
        FLAG_SET_DEFAULT(OldSize, MIN2(3*NewSize,  max_heap - NewSize));
      }
    }
  }
  // Unless explicitly requested otherwise, prefer a large
  // Old to Young gen size so as to shift the collection load
  // to the old generation concurrent collector
  if (FLAG_IS_DEFAULT(NewRatio)) {
    FLAG_SET_DEFAULT(NewRatio, MAX2(NewRatio, (intx)15));
  }
  // Unless explicitly requested otherwise, prefer to
  // promote all objects surviving a scavenge
  if (FLAG_IS_DEFAULT(MaxTenuringThreshold) &&
      FLAG_IS_DEFAULT(SurvivorRatio)) {
    FLAG_SET_DEFAULT(MaxTenuringThreshold, 0);
  }
  // If we decided above (or user explicitly requested)
  // `promote all' (via MaxTenuringThreshold := 0),
  // prefer minuscule survivor spaces so as not to waste
  // space for (non-existent) survivors
  if (FLAG_IS_DEFAULT(SurvivorRatio) && MaxTenuringThreshold == 0) {
    FLAG_SET_DEFAULT(SurvivorRatio, MAX2((intx)1024, SurvivorRatio));
  }

  // Unless explicitly requested otherwise, use promotion failure handling.
  if (FLAG_IS_DEFAULT(HandlePromotionFailure)) {
    if (UseCMSCollectionPassing &&
	UseCMSCompactAtFullCollection &&
	(CMSFullGCsBeforeCompaction == 0)) {
      FLAG_SET_DEFAULT(HandlePromotionFailure, true);
    }
  }
}

void Arguments::set_ergonomics_flags() {

  // Parallel GC is not compatible with sharing. If one specifies
  // that they want sharing explicitly, do not set ergonmics flags.
  if (DumpSharedSpaces || ForceSharedSpaces) {
    return;
  }

  if (os::is_server_class_machine()) {
    // If no other collector is requested explicitly, 
    // and UseParallelGC isn't specified one way or the other, 
    // use -XX:+UseParallelGC.
    if (!UseSerialGC &&
        !UseTrainGC &&
        !UseConcMarkSweepGC &&
        !UseParNewGC &&
        !DumpSharedSpaces &&
        FLAG_IS_DEFAULT(UseParallelGC)) {
      // Except if someone uses -Xrun to load JVMPI or hprof.
      // This might be too conservative.
      if (!init_libraries_at_startup()) {
        FLAG_SET(bool, UseParallelGC, true);
        no_shared_spaces();
      }
    }

    set_parallel_gc_flags();
  }
}

void Arguments::set_parallel_gc_flags() {
  // If no heap maximum was requested explicitly, 
  // use some reasonable fraction of the physical memory, 
  // up to a maximum of 1GB.
  if (UseParallelGC && FLAG_IS_DEFAULT(MaxHeapSize)) {
    const uint64_t reasonable_fraction = 
      os::physical_memory() / DefaultMaxRAMFraction;
    const uint64_t maximum_size = (uint64_t) DefaultMaxRAM;
    size_t reasonable_size = 
      (size_t) os::allocatable_physical_memory(reasonable_fraction);
    if (reasonable_size > maximum_size) {
      reasonable_size = maximum_size;
    }
    if (PrintGCDetails && Verbose) {
      // Cannot use gclog_or_tty yet.
      tty->print_cr("  Max heap size for serve class platform "
	SIZE_FORMAT, reasonable_size);	
    }
    FLAG_SET(uintx, MaxHeapSize, (uintx) reasonable_size);
    // If the initial_heap_size has not been set with -Xms,
    // then set it as fraction of size of physical memory
    // respecting the maximum and minimum sizes of the heap.  
    if (initial_heap_size() == 0) {
      const uint64_t reasonable_initial_fraction = 
        os::physical_memory() / DefaultInitialRAMFraction;
      const size_t reasonable_initial = 
        (size_t) os::allocatable_physical_memory(reasonable_initial_fraction);
      const size_t minimum_size = NewSize + OldSize;
      _initial_heap_size = MAX2(MIN2(reasonable_initial, reasonable_size),
			  minimum_size);
      if (PrintGCDetails && Verbose) {
	// Cannot use gclog_or_tty yet.
        tty->print_cr("  Initial heap size for serve class platform "
	  SIZE_FORMAT, _initial_heap_size);	
      }
    }
  }
}

//===========================================================================================================
// Parsing of java.compiler property and JAVA_COMPILER environment variable

void Arguments::process_java_compiler_argument(char* arg) {
  // Ignore java.compiler property and JAVA_COMPILER environment variable. 
  // It will cause the java.lang.Compiler static initializer to try to dynamically 
  // link the dll, which is not going to work since HotSpot does not support the 
  // old JIT interface. 
  // For backwards compatibility, we switch to interpreted mode if "NONE" or "" is 
  // specified.
  if (strlen(arg) == 0 || strcasecmp(arg, "NONE") == 0) {
    set_java_compiler(true);    // "-Djava.compiler[=...]" most recently seen.
  }
}

void Arguments::parse_java_compiler_environment_variable() {
  char buffer[64];
  if (os::getenv("JAVA_COMPILER", buffer, sizeof(buffer))) {
	process_java_compiler_argument(buffer);
  }
}


//===========================================================================================================
// Parsing of main arguments

bool Arguments::methodExists (char* className, char* methodName, 
  int classesNum, char** classes, bool* allMethods,
  int methodsNum, char** methods, bool* allClasses) {

  int i;
  bool classExists = false, methodExists = false;

  for (i = 0; i < classesNum; i++)
    if (strstr( className, classes[i]) != NULL) {
      if (allMethods[i])
        return true;
      classExists = true;
    }

  for (i = 0; i < methodsNum; i++)
    if (strcmp( methods[i], methodName) == 0) {
      if (allClasses[i])
        return true;
      methodExists = true;
    }

  return classExists && methodExists;
}


// Check if head of 'option' matches 'name', and sets 'tail' remaining part of option string

static bool match_option(const JavaVMOption *option, const char* name,
                         const char** tail) {  
  int len = (int)strlen(name);
  if (!strncmp(option->optionString, name, len)) {
    *tail = option->optionString + len;
    return true;
  } else {
    return false;
  }
}

bool Arguments::verify_percentage(uintx value, const char* name) {
  if (value <= 100) {
    return true;
  }
  jio_fprintf(defaultStream::error_stream(),
	      "%s of " UINTX_FORMAT " is invalid; must be between 0 and 100\n",
	      name, value);
  return false;
}

// Check the consistency of vm_init_args
bool Arguments::check_vm_args_consistency() {
  // Method for adding checks for flag consistency.
  // The intent is to warn the user of all possible conflicts,
  // before returning an error.
  // Note: Needs platform-dependent factoring.
  bool status = true;
  
  #if ( (defined(WIN32) || defined(LINUX)))
  if (UseISM || UsePermISM) {
    jio_fprintf(defaultStream::error_stream(),
		"Large pages not supported on this OS.\n");    
    status = false;
  }
  // MPSS has been set to true as default so we need to turn it off
  // for windows and linux.  We could have set the default to false 
  // and turned on the flag for Solaris but then for whatever reason,
  // there wouldn't be a mechanism to turn off MPSS for Solaris.
  FLAG_SET_DEFAULT(UseMPSS, false);
  #endif 

  // In all cases, ISM has precedence over MPSS
  if (UseISM || UsePermISM) {
    FLAG_SET_DEFAULT(UseMPSS, false);
  }

  #if ( (defined(COMPILER2) && defined(SPARC)))
  VM_Version::initialize();
  if (!VM_Version::has_v9()) {
    jio_fprintf(defaultStream::error_stream(),
		"V8 Machine detected, Server requires V9\n");
    status = false;
  }
  #endif

  // Allow both -XX:-UseStackBanging and -XX:-UseBoundThreads in non-product
  // builds so the cost of stack banging can be measured.
  #if (defined(PRODUCT) && defined(SOLARIS))
  if (!UseBoundThreads && !UseStackBanging) {
    jio_fprintf(defaultStream::error_stream(),
		"-UseStackBanging conflicts with -UseBoundThreads\n");
     
     status = false;
  }
  #endif

  if (TLABRefillWasteFraction == 0) {
    jio_fprintf(defaultStream::error_stream(),
                "TLABRefillWasteFraction should be a denominator, "
                "not " SIZE_FORMAT "\n",
                TLABRefillWasteFraction);
    status = false;
  }

  status &= verify_percentage(MaxLiveObjectEvacuationRatio,
			      "MaxLiveObjectEvacuationRatio");
  status &= verify_percentage(AdaptiveSizePolicyWeight,
			      "AdaptiveSizePolicyWeight");
  status &= verify_percentage(AdaptivePermSizeWeight, "AdaptivePermSizeWeight");
  status &= verify_percentage(ThresholdTolerance, "ThresholdTolerance");
  status &= verify_percentage(MinHeapFreeRatio, "MinHeapFreeRatio");
  status &= verify_percentage(MaxHeapFreeRatio, "MaxHeapFreeRatio");

  if (MinHeapFreeRatio > MaxHeapFreeRatio) {
    jio_fprintf(defaultStream::error_stream(),
                "MinHeapFreeRatio (" UINTX_FORMAT ") must be less than or "
		"equal to MaxHeapFreeRatio (" UINTX_FORMAT ")\n",
		MinHeapFreeRatio, MaxHeapFreeRatio);
    status = false;
  }
  // Keeping the heap 100% free is hard ;-) so limit it to 99%.
  MinHeapFreeRatio = MIN2(MinHeapFreeRatio, (uintx) 99);

  if (FullGCALot && FLAG_IS_DEFAULT(MarkSweepAlwaysCompactCount)) {
    MarkSweepAlwaysCompactCount = 1;  // Move objects every gc.
  }

  if (GCTimeLimit >= 100) {
    if ( GCTimeLimit == 100) {
      // Turn off gc-time-limit-exceeded checks
      FLAG_SET_DEFAULT(UseGCTimeLimit, false);
    } else {
      jio_fprintf(defaultStream::error_stream(),
                  "GCTimeLimit should be between 0 and 100, "
                  "not " SIZE_FORMAT "\n",
                  GCTimeLimit);
      status = false;
    }
  }

  status &= verify_percentage(GCHeapFreeLimit, "GCHeapFreeLimit");

  // Check user specified sharing option conflict with Parallel GC
  bool cannot_share = (UseConcMarkSweepGC || UseTrainGC || UseParallelGC
                       || UseISM || UsePermISM);
  if (cannot_share) {

    // Either force sharing on by forcing the other options off, or
    // force sharing off.
    if (DumpSharedSpaces || ForceSharedSpaces) {
      FLAG_SET_DEFAULT(UseSerialGC, true);
      FLAG_SET_DEFAULT(UseParNewGC, false);
      FLAG_SET_DEFAULT(UseConcMarkSweepGC, false);
      FLAG_SET_DEFAULT(UseTrainGC, false);
      FLAG_SET_DEFAULT(UseParallelGC, false);
      FLAG_SET_DEFAULT(UseISM, false);
      FLAG_SET_DEFAULT(UsePermISM, false);
    } else {
      no_shared_spaces();
    }
  }

  // Better not attempt to store into a read-only space.
  if (UseSharedSpaces) {
    FLAG_SET_DEFAULT(RewriteBytecodes, false);
    FLAG_SET_DEFAULT(RewriteFrequentPairs, false);
  }


  // Ensure that the user has not selected conflicting sets
  // of collectors. [Note: this check is merely a user convenience;
  // collectors over-ride each other so that only a non-conflicting
  // set is selected; however what the user gets is not what they
  // may have expected from the combination they asked for. It's
  // better to reduce user confusion by not allowing them to
  // select conflicting combinations.
  uint i = 0;
  if (UseSerialGC)                       i++;
  if (UseConcMarkSweepGC || UseParNewGC) i++;
  if (UseTrainGC)                        i++;
  if (UseParallelGC)                     i++;
  if (i > 1) {
    jio_fprintf(defaultStream::error_stream(),
                "Conflicting collector combinations in option list; "
                "please refer to the release notes for the combinations "
                "allowed\n");
    status = false;
  }

  if (CMSIncrementalMode) {
    if (!UseConcMarkSweepGC) {
      jio_fprintf(defaultStream::error_stream(),
		  "error:  invalid argument combination.\n"
		  "The CMS collector (-XX:+UseConcMarkSweepGC) must be "
		  "selected in order\nto use CMSIncrementalMode.\n");
      status = false;
    } else if (!UseTLAB) {
      jio_fprintf(defaultStream::error_stream(),
		  "error:  CMSIncrementalMode requires thread-local "
		  "allocation buffers\n(-XX:+UseTLAB).\n");
      status = false;
    } else {
      status &= verify_percentage(CMSIncrementalDutyCycle,
				  "CMSIncrementalDutyCycle");
      status &= verify_percentage(CMSIncrementalDutyCycleMin,
				  "CMSIncrementalDutyCycleMin");
      status &= verify_percentage(CMSIncrementalSafetyFactor,
				  "CMSIncrementalSafetyFactor");
      status &= verify_percentage(CMSIncrementalOffset,
				  "CMSIncrementalOffset");
      status &= verify_percentage(CMSExpAvgFactor,
				  "CMSExpAvgFactor");
      // If it was not set on the command line, set
      // CMSInitiatingOccupancyFraction to 1 so icms can initiate cycles early.
      if (CMSInitiatingOccupancyFraction < 0) {
	FLAG_SET_DEFAULT(CMSInitiatingOccupancyFraction, 1);
      }
    }
  }

  #ifndef PRODUCT
  // CMS space iteration, which FLSVerifyAllHeapreferences entails,
  // insists that we hold the requisite locks so that the iteration is
  // MT-safe. For the verification at start-up and shut-down, we don't
  // yet have a good way of acquiring and releasing these locks,
  // which are not visible at the CollectedHeap level. We want to
  // be able to acquire these locks and then do the iteration rather
  // than just disable the lock verification. This will be fixed under
  // bug 4788986.
  if (UseConcMarkSweepGC && FLSVerifyAllHeapReferences) {
    if (VerifyGCStartAt == 0) {
      warning("Heap verification at start-up disabled "
              "(due to current incompatibility with FLSVerifyAllHeapReferences)");
      VerifyGCStartAt = 1;      // Disable verification at start-up
    }
    if (VerifyBeforeExit) {
      warning("Heap verification at shutdown disabled "
              "(due to current incompatibility with FLSVerifyAllHeapReferences)");
      VerifyBeforeExit = false; // Disable verification at shutdown
    }
  }
  #endif // !PRODUCT

  #ifndef PRODUCT
  // CMS space iteration, which FLSVerifyAllHeapreferences entails,
  // insists that we hold the requisite locks so that the iteration is
  // MT-safe. For the verification at start-up and shut-down, we don't
  // yet have a good way of acquiring and releasing these locks,
  // which are not visible at the CollectedHeap level. We want to
  // be able to acquire these locks and then do the iteration rather
  // than just disable the lock verification. This will be fixed under
  // bug 4788986.
  if (UseConcMarkSweepGC && FLSVerifyAllHeapReferences) {
    if (VerifyGCStartAt == 0) {
      warning("Heap verification at start-up disabled "
              "(due to current incompatibility with FLSVerifyAllHeapReferences)");
      VerifyGCStartAt = 1;      // Disable verification at start-up
    }
    if (VerifyBeforeExit) {
      warning("Heap verification at shutdown disabled "
              "(due to current incompatibility with FLSVerifyAllHeapReferences)");
      VerifyBeforeExit = false; // Disable verification at shutdown
    }
  }
  #endif // !PRODUCT

  return status;
}

bool Arguments::is_bad_option(const JavaVMOption* option, jboolean ignore,
  const char* option_type) {
  if (ignore) return false;

  const char* spacer = " ";
  if (option_type == NULL) {
    option_type = ++spacer; // Set both to the empty string.
  }

  if (os::obsolete_option(option)) {
    jio_fprintf(defaultStream::error_stream(),
		"Obsolete %s%soption: %s\n", option_type, spacer,
      option->optionString);
    return false;
  } else {
    jio_fprintf(defaultStream::error_stream(),
		"Unrecognized %s%soption: %s\n", option_type, spacer,
      option->optionString);
    return true;
  }
}

static const char* user_assertion_options[] = {
  "-da", "-ea", "-disableassertions", "-enableassertions", 0
};

static const char* system_assertion_options[] = {
  "-dsa", "-esa", "-disablesystemassertions", "-enablesystemassertions", 0
};

// Return true if any of the strings in null-terminated array 'names' matches.
// If tail_allowed is true, then the tail must begin with a colon; otherwise,
// the option must match exactly.
static bool match_option(const JavaVMOption* option, const char** names, const char** tail,
  bool tail_allowed) {
  for (/* empty */; *names != NULL; ++names) {
    if (match_option(option, *names, tail)) {
      if (**tail == '\0' || tail_allowed && **tail == ':') {
	return true;
      }
    }
  }
  return false;
}

Arguments::ArgsRange Arguments::parse_memory_size(const char* s,
						  jlong* long_arg,
						  jlong min_size) {
  if (!atomll(s, long_arg)) return arg_unreadable;
  return check_memory_size(*long_arg, min_size);
}

// Parse JavaVMInitArgs structure

jint Arguments::parse_vm_init_args(const JavaVMInitArgs* args) {
  // For components of the system classpath.
  SysClassPath scp(Arguments::get_sysclasspath());
  bool scp_assembly_required = false;

  // Save default settings for some mode flags
  Arguments::_AlwaysCompileLoopMethods = AlwaysCompileLoopMethods;
  Arguments::_UseOnStackReplacement    = UseOnStackReplacement;
  Arguments::_ClipInlining             = ClipInlining;
  Arguments::_BackgroundCompilation    = BackgroundCompilation;
  Arguments::_Tier2CompileThreshold    = Tier2CompileThreshold;

  // Parse JAVA_TOOL_OPTIONS environment variable (if present) 
  jint result = parse_java_tool_options_environment_variable(&scp, &scp_assembly_required);
  if (result != JNI_OK) {
    return result;
  }

  // Parse JavaVMInitArgs structure passed in
  result = parse_each_vm_init_arg(args, &scp, &scp_assembly_required);
  if (result != JNI_OK) {
    return result;
  }

  // Parse _JAVA_OPTIONS environment variable (if present) (mimics classic VM)
  result = parse_java_options_environment_variable(&scp, &scp_assembly_required);
  if (result != JNI_OK) {
    return result;
  }

  // Do final processing now that all arguments have been parsed
  result = finalize_vm_init_args(&scp, scp_assembly_required);
  if (result != JNI_OK) {
    return result;
  }

  return JNI_OK;
}


jint Arguments::parse_each_vm_init_arg(const JavaVMInitArgs* args, SysClassPath* scp_p, bool* scp_assembly_required_p) {
  // Remaining part of option string
  const char* tail;

  // iterate over arguments  
  for (int index = 0; index < args->nOptions; index++) {
    bool is_absolute_path = false;  // for -agentpath vs -agentlib

    const JavaVMOption* option = args->options + index;    

    if (!match_option(option, "-Djava.class.path", &tail) &&
        !match_option(option, "-Dsun.java.command", &tail)) { 

        // add all jvm options to the jvm_args string. This string
        // is used later to set the java.vm.args PerfData string constant.
        // the -Djava.class.path and the -Dsun.java.command options are
        // omitted from jvm_args string as each have their own PerfData
        // string constant object.
	build_jvm_args(option->optionString);
    }

    // -verbose:[class/gc/jni]
    if (match_option(option, "-verbose", &tail)) {
      if (!strcmp(tail, ":class") || !strcmp(tail, "")) {
        FLAG_SET(bool, TraceClassLoading, true);
        FLAG_SET(bool, TraceClassUnloading, true);
      } else if (!strcmp(tail, ":gc")) {
        FLAG_SET(bool, PrintGC, true);
        FLAG_SET(bool, TraceClassUnloading, true);
      } else if (!strcmp(tail, ":jni")) {
        FLAG_SET(bool, PrintJNIResolving, true);
      }    
    // -da / -ea / -disableassertions / -enableassertions
    // These accept an optional class/package name separated by a colon, e.g.,
    // -da:java.lang.Thread.
    } else if (match_option(option, user_assertion_options, &tail, true)) {
      bool enable = option->optionString[1] == 'e';	// char after '-' is 'e'
      if (*tail == '\0') {
	JavaAssertions::setUserClassDefault(enable);
      } else {
	assert(*tail == ':', "bogus match by match_option()");
	JavaAssertions::addOption(tail + 1, enable);
      }
    // -dsa / -esa / -disablesystemassertions / -enablesystemassertions
    } else if (match_option(option, system_assertion_options, &tail, false)) {
      bool enable = option->optionString[1] == 'e';	// char after '-' is 'e'
      JavaAssertions::setSystemClassDefault(enable);
    // -bootclasspath:
    } else if (match_option(option, "-Xbootclasspath:", &tail)) {
      scp_p->reset_path(tail);
      *scp_assembly_required_p = true;
    // -bootclasspath/a:
    } else if (match_option(option, "-Xbootclasspath/a:", &tail)) {
      scp_p->add_suffix(tail);
      *scp_assembly_required_p = true;
    // -bootclasspath/p:
    } else if (match_option(option, "-Xbootclasspath/p:", &tail)) {
      scp_p->add_prefix(tail);
      *scp_assembly_required_p = true;
    // -Xrun
    } else if (match_option(option, "-Xrun", &tail)) {
      if(tail != NULL) {
        const char* pos = strchr(tail, ':');
        size_t len = (pos == NULL) ? strlen(tail) : pos - tail;
        char* name = (char*)memcpy(NEW_C_HEAP_ARRAY(char, len + 1), tail, len);
        name[len] = '\0';

        char *options = NULL;
        if(pos != NULL) {
          size_t len2 = strlen(pos+1) + 1; // options start after ':'.  Final zero must be copied.
          options = (char*)memcpy(NEW_C_HEAP_ARRAY(char, len2), pos+1, len2);
        }
        add_init_library(name, options);
      }
    // -agentlib and -agentpath
    } else if (match_option(option, "-agentlib:", &tail) ||
          (is_absolute_path = match_option(option, "-agentpath:", &tail))) {
      if(tail != NULL) {
        const char* pos = strchr(tail, '=');
        size_t len = (pos == NULL) ? strlen(tail) : pos - tail;
        char* name = strncpy(NEW_C_HEAP_ARRAY(char, len + 1), tail, len);
        name[len] = '\0';

        char *options = NULL;
        if(pos != NULL) {
          options = strcpy(NEW_C_HEAP_ARRAY(char, strlen(pos + 1) + 1), pos + 1);
        }
        add_init_agent(name, options, is_absolute_path);
      }
    // -javaagent
    } else if (match_option(option, "-javaagent:", &tail)) {
      if(tail != NULL) {
        char *options = strcpy(NEW_C_HEAP_ARRAY(char, strlen(tail) + 1), tail);
        add_init_agent("instrument", options, false);
      }
    // -Xnoclassgc
    } else if (match_option(option, "-Xnoclassgc", &tail)) {
      FLAG_SET(bool, ClassUnloading, false);
    // -Xincgc
    } else if (match_option(option, "-Xincgc", &tail)) {
      FLAG_SET(bool, UseConcMarkSweepGC, true);
    // -Xnoincgc
    } else if (match_option(option, "-Xnoincgc", &tail)) {
      FLAG_SET(bool, UseConcMarkSweepGC, false);
    // -Xconcgc
    } else if (match_option(option, "-Xconcgc", &tail)) {
      FLAG_SET(bool, UseConcMarkSweepGC, true);
    // -Xnoconcgc
    } else if (match_option(option, "-Xnoconcgc", &tail)) {
      FLAG_SET(bool, UseConcMarkSweepGC, false);
    // -Xbatch
    } else if (match_option(option, "-Xbatch", &tail)) {
      FLAG_SET(bool, BackgroundCompilation, false);
    // -Xmn for compatibility with other JVM vendors
    } else if (match_option(option, "-Xmn", &tail)) {
      jlong long_initial_eden_size = 0;
      ArgsRange errcode = parse_memory_size(tail, &long_initial_eden_size, 1);
      if (errcode != arg_in_range) {
        jio_fprintf(defaultStream::error_stream(),
		    "Invalid initial eden size: %s\n", option->optionString);
        describe_range_error(errcode);
        return JNI_EINVAL;
      }
      FLAG_SET(uintx, MaxNewSize, (size_t) long_initial_eden_size);
      FLAG_SET(uintx, NewSize, (size_t) long_initial_eden_size);
    // -Xms
    } else if (match_option(option, "-Xms", &tail)) {
      jlong long_initial_heap_size = 0;
      ArgsRange errcode = parse_memory_size(tail, &long_initial_heap_size, 1);
      if (errcode != arg_in_range) {
        jio_fprintf(defaultStream::error_stream(),
		    "Invalid initial heap size: %s\n", option->optionString);
        describe_range_error(errcode);
        return JNI_EINVAL;
      }
      _initial_heap_size = (size_t) long_initial_heap_size;
    // -Xmx
    } else if (match_option(option, "-Xmx", &tail)) {
      jlong long_max_heap_size = 0;
      ArgsRange errcode = parse_memory_size(tail, &long_max_heap_size, 1);
      if (errcode != arg_in_range) {
        jio_fprintf(defaultStream::error_stream(),
		    "Invalid maximum heap size: %s\n", option->optionString);
        describe_range_error(errcode);
        return JNI_EINVAL;
      }
      FLAG_SET(uintx, MaxHeapSize, (size_t) long_max_heap_size);
    // Xmaxf
    } else if (match_option(option, "-Xmaxf", &tail)) {
      int maxf = atof(tail) * 100;
      if (maxf < 0 || maxf > 100) {
        jio_fprintf(defaultStream::error_stream(),
		    "Bad max heap free percentage size: %s\n",
		    option->optionString);
        return JNI_EINVAL;
      } else {
        FLAG_SET(uintx, MaxHeapFreeRatio, maxf);
      }
    // Xminf
    } else if (match_option(option, "-Xminf", &tail)) {
      int minf = atof(tail) * 100;
      if (minf < 0 || minf > 100) {
        jio_fprintf(defaultStream::error_stream(),
		    "Bad min heap free percentage size: %s\n",
		    option->optionString);
        return JNI_EINVAL;
      } else {
        FLAG_SET(uintx, MinHeapFreeRatio, minf);
      }
    // -Xss
    } else if (match_option(option, "-Xss", &tail)) {
      jlong long_ThreadStackSize = 0;
      ArgsRange errcode = parse_memory_size(tail, &long_ThreadStackSize, 1000);
      if (errcode != arg_in_range) {
        jio_fprintf(defaultStream::error_stream(),
		    "Invalid thread stack size: %s\n", option->optionString);
        describe_range_error(errcode);
        return JNI_EINVAL;
      }
      // Internally track ThreadStackSize in units of 1024 bytes.
      FLAG_SET(intx, ThreadStackSize,
                              round_to((int)long_ThreadStackSize, K) / K);
    // -Xoss
    } else if (match_option(option, "-Xoss", &tail)) {
	  // HotSpot does not have separate native and Java stacks, ignore silently for compatibility
    // -Xmaxjitcodesize
    } else if (match_option(option, "-Xmaxjitcodesize", &tail)) {
      jlong long_ReservedCodeCacheSize = 0;
      ArgsRange errcode = parse_memory_size(tail, &long_ReservedCodeCacheSize,
					    InitialCodeCacheSize);
      if (errcode != arg_in_range) {
        jio_fprintf(defaultStream::error_stream(),
		    "Invalid maximum code cache size: %s\n",
		    option->optionString);
        describe_range_error(errcode);
        return JNI_EINVAL;
      }
      FLAG_SET(intx, ReservedCodeCacheSize, (int) long_ReservedCodeCacheSize);
    // -green
    } else if (match_option(option, "-green", &tail)) {
      jio_fprintf(defaultStream::error_stream(),
		  "Green threads support not available\n");
	  return JNI_EINVAL;
    // -native
    } else if (match_option(option, "-native", &tail)) {
	  // HotSpot always uses native threads, ignore silently for compatibility
    // -Xsqnopause
    } else if (match_option(option, "-Xsqnopause", &tail)) {
	  // EVM option, ignore silently for compatibility
    // -Xrs
    } else if (match_option(option, "-Xrs", &tail)) {
	  // Classic/EVM option, new functionality
      FLAG_SET(bool, ReduceSignalUsage, true);
    } else if (match_option(option, "-Xusealtsigs", &tail)) {
          // change default internal VM signals used - lower case for back compat
      FLAG_SET(bool, UseAltSigs, true);
    // -Xoptimize
    } else if (match_option(option, "-Xoptimize", &tail)) {
	  // EVM option, ignore silently for compatibility
    // -Xprof
    } else if (match_option(option, "-Xprof", &tail)) {
      _has_profile = true;
    // -Xaprof
    } else if (match_option(option, "-Xaprof", &tail)) {
      _has_alloc_profile = true;
    // -Xconcurrentio
    } else if (match_option(option, "-Xconcurrentio", &tail)) {
      FLAG_SET(bool, UseLWPSynchronization, true);
      FLAG_SET(bool, BackgroundCompilation, false);
      FLAG_SET(intx, DeferThrSuspendLoopCount, 1);
      FLAG_SET(bool, UseTLAB, false);
      FLAG_SET(uintx, NewSizeThreadIncrease, 16 * K);  // 20Kb per thread added to new generation

      // -Xinternalversion
    } else if (match_option(option, "-Xinternalversion", &tail)) {
      jio_fprintf(defaultStream::output_stream(), "%s\n",
		  VM_Version::internal_vm_info_string());
      vm_exit(0);
#ifndef PRODUCT
    // -Xprintflags
    } else if (match_option(option, "-Xprintflags", &tail)) {
      CommandLineFlags::printFlags();
      vm_exit(0);
#endif
    // -D
    } else if (match_option(option, "-D", &tail)) {      
      if (!add_property(tail)) {
        return JNI_ENOMEM;
      }
      // Out of the box management support
      if (match_option(option, "-Dcom.sun.management", &tail)) {
        FLAG_SET(bool, ManagementServer, true);
      }
    // -Xint
    } else if (match_option(option, "-Xint", &tail)) {
	  set_mode_flags(_int);
    // -Xmixed
    } else if (match_option(option, "-Xmixed", &tail)) {
	  set_mode_flags(_mixed);
    // -Xcomp
    } else if (match_option(option, "-Xcomp", &tail)) {
      // for testing the compiler; turn off all flags that inhibit compilation
	  set_mode_flags(_comp);

    // -Xshare:dump
    } else if (match_option(option, "-Xshare:dump", &tail)) {
#ifdef COMPILER2
      vm_exit_during_initialization(
          "Dumping a shared archive is not supported on the Server JVM.", NULL);
#else
      FLAG_SET(bool, DumpSharedSpaces, true);
      set_mode_flags(_int);	// Prevent compilation, which creates objects
#endif
    // -Xshare:on
    } else if (match_option(option, "-Xshare:on", &tail)) {
      FLAG_SET(bool, UseSharedSpaces, true);
      FLAG_SET(bool, RequireSharedSpaces, true);
    // -Xshare:auto
    } else if (match_option(option, "-Xshare:auto", &tail)) {
      FLAG_SET(bool, UseSharedSpaces, true);
      FLAG_SET(bool, RequireSharedSpaces, false);
    // -Xshare:off
    } else if (match_option(option, "-Xshare:off", &tail)) {
      FLAG_SET(bool, UseSharedSpaces, false);
      FLAG_SET(bool, RequireSharedSpaces, false);

    // -Xverify
    } else if (match_option(option, "-Xverify", &tail)) {      
      if (strcmp(tail, ":all") == 0 || strcmp(tail, "") == 0) {
        FLAG_SET(bool, BytecodeVerificationLocal, true);
        FLAG_SET(bool, BytecodeVerificationRemote, true);
      } else if (strcmp(tail, ":remote") == 0) {
        FLAG_SET(bool, BytecodeVerificationLocal, false);
        FLAG_SET(bool, BytecodeVerificationRemote, true);
      } else if (strcmp(tail, ":none") == 0) {
        FLAG_SET(bool, BytecodeVerificationLocal, false);
        FLAG_SET(bool, BytecodeVerificationRemote, false);
      } else if (is_bad_option(option, args->ignoreUnrecognized, "verification")) {
	return JNI_EINVAL;
      }
    // -Xdebug
    } else if (match_option(option, "-Xdebug", &tail)) {
      JvmtiExport::create_jvmdi_interface();
    // -Xnoagent 
    } else if (match_option(option, "-Xnoagent", &tail)) {    
      // For compatibility with classic. HotSpot refuses to load the old style agent.dll.
    } else if (match_option(option, "-Xboundthreads", &tail)) {    
      // Bind user level threads to kernel threads (Solaris only)
      FLAG_SET(bool, UseBoundThreads, true);
    } else if (match_option(option, "-Xloggc:", &tail)) {
      // Redirect GC output to the file. -Xloggc:<filename>
      // ostream_init_log(), when called will use this filename
      // to initialize a fileStream.
      _gc_log_filename = tail;
      FLAG_SET(bool, PrintGC, true);
      FLAG_SET(bool, PrintGCTimeStamps, true);
      FLAG_SET(bool, TraceClassUnloading, true);

    // JNI hooks
    } else if (match_option(option, "-Xcheck", &tail)) {    
      if (!strcmp(tail, ":jni")) {
        CheckJNICalls = true;
      } else if (is_bad_option(option, args->ignoreUnrecognized, 
                                     "check")) {
        return JNI_EINVAL;
      }
    } else if (match_option(option, "vfprintf", &tail)) {    
      _vfprintf_hook = CAST_TO_FN_PTR(vfprintf_hook_t, option->extraInfo);
    } else if (match_option(option, "exit", &tail)) {    
      _exit_hook = CAST_TO_FN_PTR(exit_hook_t, option->extraInfo);
    } else if (match_option(option, "abort", &tail)) {    
      _abort_hook = CAST_TO_FN_PTR(abort_hook_t, option->extraInfo);
    // -XX:+AggressiveHeap
    } else if (match_option(option, "-XX:+AggressiveHeap", &tail)) {

      // This option inspects the machine and attempts to set various
      // parameters to be optimal for long-running, memory allocation
      // intensive jobs.  It is intended for machines with large
      // amounts of cpu and memory.

      // initHeapSize is needed since _initial_heap_size is 4 bytes on a 32 bit
      // VM, but we may not be able to represent the total physical memory
      // available (like having 8gb of memory on a box but using a 32bit VM).
      // Thus, we need to make sure we're using a julong for intermediate
      // calculations.
      julong initHeapSize;
      julong total_memory = os::physical_memory();

      if (total_memory < (julong)256*M) {
        jio_fprintf(defaultStream::error_stream(),
		    "You need at least 256mb of memory to use -XX:+AggressiveHeap\n");
        vm_exit(1);
      }

      // The heap size is half of available memory, or (at most)
      // all of possible memory less 160mb (leaving room for the OS
      // when using ISM).  This is the maximum; because adaptive sizing
      // is turned on below, the actual space used may be smaller.

      initHeapSize = MIN2(total_memory / (julong)2,
                          total_memory - (julong)160*M);

      // Make sure that if we have a lot of memory we cap the 32 bit
      // process space.  The 64bit VM version of this function is a nop.
      initHeapSize = os::allocatable_physical_memory(initHeapSize);

      // The perm gen is separate but contiguous with the
      // object heap (and is reserved with it) so subtract it
      // from the heap size.
      if (initHeapSize > MaxPermSize) {
        initHeapSize = initHeapSize - MaxPermSize;
      } else {
	warning("AggressiveHeap and MaxPermSize values may conflict");
      }

      if (FLAG_IS_DEFAULT(MaxHeapSize)) {
         FLAG_SET(uintx, MaxHeapSize, initHeapSize);
         _initial_heap_size = MaxHeapSize;
      }
      if (FLAG_IS_DEFAULT(NewSize)) {
         // Make the young generation 3/8ths of the total heap.
         FLAG_SET(uintx, NewSize,
                                ((julong)MaxHeapSize / (julong)8) * (julong)3);
         FLAG_SET(uintx, MaxNewSize, NewSize);
      }

      // Enable TLABs
      FLAG_SET(bool, UseTLAB, true);

      // Increase some data structure sizes for efficiency
      FLAG_SET(uintx, BaseFootPrintEstimate, MaxHeapSize);
      FLAG_SET(bool, ResizeTLAB, false);
      FLAG_SET(uintx, TLABSize, 256*K);
      FLAG_SET(uintx, YoungPLABSize, 256*K);      // Note: this is in words

      // Enable parallel GC and adaptive generation sizing
      FLAG_SET(bool, UseParallelGC, true);

      FLAG_SET(bool, UseAdaptiveSizePolicy, true);

      FLAG_SET(uintx, ParallelGCThreads, nof_parallel_gc_threads());

      // Encourage steady state memory management
      FLAG_SET(uintx, ThresholdTolerance, 100);

      // This appears to improve mutator locality
      FLAG_SET(bool, ScavengeBeforeFullGC, false);
      
      // Get around early Solaris scheduling bug 
      // (affinity vs other jobs on system)
      // but disallow DR and offlining (5008695).
      FLAG_SET(bool, BindGCTaskThreadsToCPUs, true);

    } else if (match_option(option, "-XX:+NeverTenure", &tail)) {    
      // The last option must always win.
      FLAG_SET(bool, AlwaysTenure, false);
      FLAG_SET(bool, NeverTenure, true);
    } else if (match_option(option, "-XX:+AlwaysTenure", &tail)) {    
      // The last option must always win.
      FLAG_SET(bool, NeverTenure, false);
      FLAG_SET(bool, AlwaysTenure, true);
    } else if (match_option(option, "-XX:-UseParallelGC", &tail)) {    
      // We need to make sure the parallel thread count is reset to zero
      FLAG_SET(bool, UseParallelGC, false);
      FLAG_SET(uintx, ParallelGCThreads, 0);
    // The TLE options are for compatibility with 1.3 and will be
    // removed without notice in a future release.  These options
    // are not to be documented.
    } else if (match_option(option, "-XX:MaxTLERatio=", &tail)) {
      // No longer used.
    } else if (match_option(option, "-XX:+ResizeTLE", &tail)) {
      FLAG_SET(bool, ResizeTLAB, true);
    } else if (match_option(option, "-XX:-ResizeTLE", &tail)) {
      FLAG_SET(bool, ResizeTLAB, false);
    } else if (match_option(option, "-XX:+PrintTLE", &tail)) {
      FLAG_SET(bool, PrintTLAB, true);
    } else if (match_option(option, "-XX:-PrintTLE", &tail)) {
      FLAG_SET(bool, PrintTLAB, false);
    } else if (match_option(option, "-XX:TLEFragmentationRatio=", &tail)) {
      // No longer used.
    } else if (match_option(option, "-XX:TLESize=", &tail)) {
      jlong long_tlab_size = 0;
      ArgsRange errcode = parse_memory_size(tail, &long_tlab_size, 1);
      if (errcode != arg_in_range) {
        jio_fprintf(defaultStream::error_stream(),
		    "Invalid TLAB size: %s\n", option->optionString);
        describe_range_error(errcode);
        return JNI_EINVAL;
      }
      FLAG_SET(uintx, TLABSize, long_tlab_size);
    } else if (match_option(option, "-XX:TLEThreadRatio=", &tail)) {
      // No longer used.
    } else if (match_option(option, "-XX:+UseTLE", &tail)) {
      FLAG_SET(bool, UseTLAB, true);
    } else if (match_option(option, "-XX:-UseTLE", &tail)) {
      FLAG_SET(bool, UseTLAB, false);
    } else if (match_option(option, "-XX:+DisplayVMOutputToStderr", &tail)) {
      FLAG_SET(bool, DisplayVMOutputToStdout, false);
      FLAG_SET(bool, DisplayVMOutputToStderr, true);
    } else if (match_option(option, "-XX:+DisplayVMOutputToStdout", &tail)) {
      FLAG_SET(bool, DisplayVMOutputToStderr, false);
      FLAG_SET(bool, DisplayVMOutputToStdout, true);
    } else if (match_option(option, "-XX:", &tail)) { // -XX:xxxx
      // Skip -XX:Flags= since that case has already been handled
      if (strncmp(tail, "Flags=", strlen("Flags=")) != 0) {
        if (!process_argument(tail, args->ignoreUnrecognized)) {
          return JNI_EINVAL;
        }
      }
    // Unknown option
    } else if (is_bad_option(option, args->ignoreUnrecognized)) {
      return JNI_ERR;
    }
  }

  return JNI_OK;
}

jint Arguments::finalize_vm_init_args(SysClassPath* scp_p, bool scp_assembly_required) {
  // This must be done after all -D arguments have been processed.
  scp_p->expand_endorsed();

  if (scp_assembly_required || scp_p->get_endorsed() != NULL) {
    // Assemble the bootclasspath elements into the final path.
    Arguments::set_sysclasspath(scp_p->combined_path());
  }

  // This must be done after all arguments have been processed.
  // java_compiler() true means set to "NONE" or empty.
  // The -Xdebug mini-agent sets the breakpoint capability (amongst many).
  if (java_compiler() && !JvmtiExport::can_post_breakpoint()) {
    // For backwards compatibility, we switch to interpreted mode if
    // -Djava.compiler="NONE" or "" is specified AND "-Xdebug" was
    // not specified.
    set_mode_flags(_int);
  }

  if (!check_vm_args_consistency()) {
    return JNI_ERR;
  }

  return JNI_OK;
}


jint Arguments::parse_java_options_environment_variable(SysClassPath* scp_p, bool* scp_assembly_required_p) {
  char buffer[1024];

  // Don't check this variable if user has special privileges
  // (e.g. unix su command).

  if (os::getenv("_JAVA_OPTIONS", buffer, sizeof(buffer)) &&
      !os::have_special_privileges()) {
    const int N_MAX_OPTIONS = 32;
    // Construct JavaVMOption array
    JavaVMOption options[N_MAX_OPTIONS];
    jio_fprintf(defaultStream::error_stream(),
		"Picked up _JAVA_OPTIONS: %s\n", buffer);
    char* c = buffer;
    int i;
    for (i = 0; i < N_MAX_OPTIONS;) {
      // Skip whitespace
      while (isspace(*c)) c++;
      if (*c == 0) break;
      // Fill in option
      options[i++].optionString = c;
      while (*c != 0 && !isspace(*c)) c++;
      // Check for end
      if (*c == 0) break;
      // Zero terminate option
      *c++ = 0;
    }
    // Construct JavaVMInitArgs structure
    JavaVMInitArgs vm_args;
    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = options;
    vm_args.nOptions = i;
    vm_args.ignoreUnrecognized = false;
    return(parse_each_vm_init_arg(&vm_args, scp_p, scp_assembly_required_p));
  }
  return JNI_OK;
}


jint Arguments::parse_java_tool_options_environment_variable(SysClassPath* scp_p, bool* scp_assembly_required_p) {
  const int N_MAX_OPTIONS = 64;
  const int OPTION_BUFFER_SIZE = 1024;
  char buffer[OPTION_BUFFER_SIZE];

  // The variable will be ignored if it exceeds the length of the buffer.
  // Don't check this variable if user has special privileges
  // (e.g. unix su command).
  if (os::getenv("JAVA_TOOL_OPTIONS", buffer, sizeof(buffer)) &&
      !os::have_special_privileges()) {
    JavaVMOption options[N_MAX_OPTIONS];      // Construct option array
    jio_fprintf(defaultStream::error_stream(),
		"Picked up JAVA_TOOL_OPTIONS: %s\n", buffer);
    char* rd = buffer;                        // pointer to the input string (rd)
    int i;
    for (i = 0; i < N_MAX_OPTIONS;) {         // repeat for all options in the input string
      while (isspace(*rd)) rd++;              // skip whitespace
      if (*rd == 0) break;                    // we re done when the input string is read completely

      // The output, option string, overwrites the input string.
      // Because of quoting, the pointer to the option string (wrt) may lag the pointer to 
      // input string (rd).
      char* wrt = rd;

      options[i++].optionString = wrt;        // Fill in option
      while (*rd != 0 && !isspace(*rd)) {     // unquoted strings terminate with a space or NULL
        if (*rd == '\'' || *rd == '"') {      // handle a quoted string
          int quote = *rd;                    // matching quote to look for
          rd++;                               // don't copy open quote
          while (*rd != quote) {              // include everything (even spaces) up until quote
            if (*rd == 0) {                   // string termination means unmatched string
              jio_fprintf(defaultStream::error_stream(), "Unmatched quote in JAVA_TOOL_OPTIONS\n");
              return JNI_ERR;
            }
            *wrt++ = *rd++;                   // copy to option string
          }
          rd++;                               // don't copy close quote
        } else {
          *wrt++ = *rd++;                     // copy to option string
        }
      }
      *wrt = 0;                               // Zero terminate option
    }
    // Construct JavaVMInitArgs structure and parse as if it was part of the command line
    JavaVMInitArgs vm_args;
    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = options;
    vm_args.nOptions = i;
    vm_args.ignoreUnrecognized = false;
    return(parse_each_vm_init_arg(&vm_args, scp_p, scp_assembly_required_p));
  }
  return JNI_OK;
}


// Parse entry point called from JNI_CreateJavaVM

jint Arguments::parse(const JavaVMInitArgs* args) {

  // Sharing support
  // Construct the path to the archive
  char jvm_path[JVM_MAXPATHLEN];
  os::jvm_path(jvm_path, sizeof(jvm_path));
  char *end = strrchr(jvm_path, *os::file_separator());
  if (end != NULL) *end = '\0';
  char *shared_archive_path = NEW_C_HEAP_ARRAY(char, strlen(jvm_path) +
                                        strlen(os::file_separator()) + 20);
  if (shared_archive_path == NULL) return JNI_ENOMEM;
  strcpy(shared_archive_path, jvm_path);
  strcat(shared_archive_path, os::file_separator());
  strcat(shared_archive_path, "classes");
  DEBUG_ONLY(strcat(shared_archive_path, "_g");)
  strcat(shared_archive_path, ".jsa");
  SharedArchivePath = shared_archive_path;

  // Remaining part of option string
  const char* tail;
  
  // If flag "-XX:Flags=flags-file" is used it will be the first option to be processed.
  bool settings_file_specified = false;
  for (int index = 0; index < args->nOptions; index++) {
    const JavaVMOption *option = args->options + index;
    if (match_option(option, "-XX:Flags=", &tail)) {
      if (!process_settings_file(tail, true, args->ignoreUnrecognized)) {
        return JNI_EINVAL;
      }
      settings_file_specified = true;
    } 
  }

  // Parse default .hotspotrc settings file
  if (!settings_file_specified) {
    if (!process_settings_file(".hotspotrc", false, args->ignoreUnrecognized)) {
      return JNI_EINVAL;
    }
  }

  // Parse JavaVMInitArgs structure passed in, as well as JAVA_TOOL_OPTIONS and _JAVA_OPTIONS
  jint result = parse_vm_init_args(args);
  if (result != JNI_OK) {
    return result;
  }

  // Parse JAVA_COMPILER environment variable (if present) (mimics classic VM)
  parse_java_compiler_environment_variable();

#ifndef PRODUCT
  if (TraceBytecodesAt != 0) {
    TraceBytecodes = true;
  }
  if (CountCompiledCalls) {
#ifdef CORE
    warning("CountCalls is set but may not work (no invocation counters in CORE build)");
#else
    if (UseCounterDecay) {
      warning("UseCounterDecay disabled because CountCalls is set");
      UseCounterDecay = false;
    }
#endif // CORE
  }
#endif // PRODUCT

  if (!RewriteBytecodes) {
    RewriteFrequentPairs = false;
  }

  if (PrintGCDetails) {
    // Turn on -verbose:gc options as well
    PrintGC = true;
    if (FLAG_IS_DEFAULT(TraceClassUnloading)) {
      TraceClassUnloading = true;
    }
  }

  // Set some flgags for ParallelGC if needed.
  set_parallel_gc_flags();

  // Set some flags for CMS and/or ParNew collectors, as needed.
  set_cms_and_parnew_gc_flags();

  // Set flags based on ergonomics.
  set_ergonomics_flags();

  // For extra robustness
  CORE_ONLY(set_mode_flags(_int));

  if (PrintCommandLineFlags) {
    CommandLineFlags::printSetFlags();
  }

  return JNI_OK;
}

// %%% Should be Arguments::GetCheckCompileOnly(), Arguments::CompileMethod().

bool CheckCompileOnly () { return Arguments::GetCheckCompileOnly();}

bool CompileMethod (char* className, char* methodName) {
  return Arguments::CompileMethod( className, methodName);
}

int Arguments::PropertyList_count(SystemProperty* pl) {
  int count = 0;
  while(pl != NULL) {
    count++;
    pl = pl->next();
  }
  return count;
}

const char* Arguments::PropertyList_get_value(SystemProperty *pl, const char* key) {
  assert(key != NULL, "just checking");
  SystemProperty* prop;
  for (prop = pl; prop != NULL; prop = prop->next()) {
    if (strcmp(key, prop->key()) == 0) return prop->value();
  }
  return NULL;
}

const char* Arguments::PropertyList_get_key_at(SystemProperty *pl, int index) {
  int count = 0;
  const char* ret_val = NULL;

  while(pl != NULL) {
    if(count >= index) {
      ret_val = pl->key();
      break;
    }
    count++;
    pl = pl->next();
  }

  return ret_val;
}

char* Arguments::PropertyList_get_value_at(SystemProperty* pl, int index) {
  int count = 0;
  char* ret_val = NULL;

  while(pl != NULL) {
    if(count >= index) {
      ret_val = pl->value();
      break;
    }
    count++;
    pl = pl->next();
  }

  return ret_val;
}

void Arguments::PropertyList_add(SystemProperty** plist, SystemProperty *new_p) {
  SystemProperty* p = *plist;
  if (p == NULL) {
    *plist = new_p;
  } else {
    while (p->next() != NULL) {
      p = p->next();
    }
    p->set_next(new_p);
  }
}

void Arguments::PropertyList_add(SystemProperty** plist, const char* k, char* v) {
  if (plist == NULL)
    return;

  SystemProperty* new_p = new SystemProperty(k, v, true);
  PropertyList_add(plist, new_p);
}

// This add maintains unique property key in the list.
void Arguments::PropertyList_unique_add(SystemProperty** plist, const char* k, char* v) {
  if (plist == NULL)
    return;

  // If property key exist then update with new value.
  SystemProperty* prop;
  for (prop = *plist; prop != NULL; prop = prop->next()) {
    if (strcmp(k, prop->key()) == 0) {
      prop->set_value(v);
      return;
    }
  }
      
  PropertyList_add(plist, k, v);
}

