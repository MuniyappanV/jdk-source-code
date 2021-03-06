#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)main.cpp	1.89 03/12/23 16:38:52 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// MAIN.CPP - Entry point for the Architecture Description Language Compiler
#include "adlc.hpp"

//------------------------------Prototypes-------------------------------------
static void  usage(ArchDesc& AD);          // Print usage message and exit
static char *strip_ext(char *fname);       // Strip off name extension
static char *base_plus_suffix(const char* base, const char *suffix);// New concatenated string 
static char *prefix_plus_base_plus_suffix(const char* prefix, const char* base, const char *suffix);// New concatenated string 

//------------------------------main-------------------------------------------
int main(int argc, char *argv[])
{
  // ResourceMark  mark;
  ADLParser    *ADL_Parse;      // ADL Parser object to parse AD file
  ArchDesc      AD;             // Architecture Description object

  // Check for proper arguments
  if( argc == 1 ) usage(AD);    // No arguments?  Then print usage

  // Read command line arguments and file names
  for( int i = 1; i < argc; i++ ) { // For all arguments
    register char *s = argv[i];	// Get option/filename

    if( *s++ == '-' ) {         // It's a flag? (not a filename)
      if( !*s ) {		// Stand-alone `-' means stdin
	//********** INSERT CODE HERE **********	
      } else while (*s != '\0') { // While have flags on option
	switch (*s++) { 	// Handle flag
	case 'd':               // Debug flag
	  AD._dfa_debug += 1;   // Set Debug Flag
	  break;
	case 'g':               // Debug ad location flag
	  AD._adlocation_debug += 1;       // Set Debug ad location Flag
	  break;
	case 'o':               // No Output Flag
	  AD._no_output ^= 1;   // Toggle no_output flag
	  break;
	case 'q':               // Quiet Mode Flag
	  AD._quiet_mode ^= 1;  // Toggle quiet_mode flag
	  break;
	case 'w':               // Disable Warnings Flag
	  AD._disable_warnings ^= 1; // Toggle disable_warnings flag
	  break;
	case 'T':               // Option to make DFA as many subroutine calls.
	  AD._dfa_small += 1;   // Set Mode Flag
	  break;
	case 'c': {             // Set C++ Output file name
	  AD._CPP_file._name = s;
          const char *base = strip_ext(strdup(s));
          AD._CPP_CLONE_file._name    = base_plus_suffix(base,"_clone.cpp");
          AD._CPP_EXPAND_file._name   = base_plus_suffix(base,"_expand.cpp");
          AD._CPP_FORMAT_file._name   = base_plus_suffix(base,"_format.cpp");
          AD._CPP_GEN_file._name      = base_plus_suffix(base,"_gen.cpp");
          AD._CPP_MISC_file._name     = base_plus_suffix(base,"_misc.cpp");
          AD._CPP_PEEPHOLE_file._name = base_plus_suffix(base,"_peephole.cpp");
          AD._CPP_PIPELINE_file._name = base_plus_suffix(base,"_pipeline.cpp");
          s += strlen(s);
	  break;
        }
	case 'h':               // Set C++ Output file name
	  AD._HPP_file._name = s; s += strlen(s);
	  break;
	case 'v':               // Set C++ Output file name
	  AD._VM_file._name = s; s += strlen(s);
	  break;
	case 'a':               // Set C++ Output file name
	  AD._DFA_file._name = s;
	  AD._bug_file._name = s;
	  s += strlen(s);
	  break;
	case '#':               // Special internal debug flag
	  AD._adl_debug++;      // Increment internal debug level
	  break;
	case 's':               // Output which instructions are cisc-spillable
	  AD._cisc_spill_debug = true;
	  break;
	case 'D':               // Flag Definition
	  {
	    char* flag = s;
	    s += strlen(s);
	    char* def = strchr(flag, '=');
	    if (def == NULL)  def = (char*)"1";
	    else              *def++ = '\0';
	    AD.set_preproc_def(flag, def);
	  }
	  break;
	case 'U':               // Flag Un-Definition
	  {
	    char* flag = s;
	    s += strlen(s);
	    AD.set_preproc_def(flag, NULL);
	  }
	  break;
	default:		// Unknown option
	  usage(AD);            // So print usage and exit
	}			// End of switch on options...
      } 			// End of while have options...

    } else {			// Not an option; must be a filename
      AD._ADL_file._name = argv[i]; // Set the input filename

      // // Files for storage, based on input file name
      const char *base = strip_ext(strdup(argv[i]));
      char       *temp = base_plus_suffix("dfa_",base);
      AD._DFA_file._name = base_plus_suffix(temp,".cpp");
      delete temp;
      temp = base_plus_suffix("ad_",base);
      AD._CPP_file._name          = base_plus_suffix(temp,".cpp");
      AD._CPP_CLONE_file._name    = base_plus_suffix(temp,"_clone.cpp");
      AD._CPP_EXPAND_file._name   = base_plus_suffix(temp,"_expand.cpp");
      AD._CPP_FORMAT_file._name   = base_plus_suffix(temp,"_format.cpp");
      AD._CPP_GEN_file._name      = base_plus_suffix(temp,"_gen.cpp");
      AD._CPP_MISC_file._name     = base_plus_suffix(temp,"_misc.cpp");
      AD._CPP_PEEPHOLE_file._name = base_plus_suffix(temp,"_peephole.cpp");
      AD._CPP_PIPELINE_file._name = base_plus_suffix(temp,"_pipeline.cpp");
      AD._HPP_file._name = base_plus_suffix(temp,".hpp");
      delete temp;
      temp = base_plus_suffix("adGlobals_",base);
      AD._VM_file._name = base_plus_suffix(temp,".hpp");
      delete temp;      
      temp = base_plus_suffix("bugs_",base);
      AD._bug_file._name = base_plus_suffix(temp,".out");
      delete temp;
    }				// End of files vs options...
  }				// End of while have command line arguments

  // Open files used to store the matcher and its components
  if (AD.open_files() == 0) return 1; // Open all input/output files

  // Build the File Buffer, Parse the input, & Generate Code
  FileBuff  ADL_Buf(&AD._ADL_file, AD); // Create a file buffer for input file

  ADL_Parse = new ADLParser(ADL_Buf, AD); // Create a parser to parse the buffer
  ADL_Parse->parse();           // Parse buffer & build description lists

  if( AD._dfa_debug >= 1 ) {    // For higher debug settings, print dump
    AD.dump();
  }

  delete ADL_Parse;             // Delete parser

  // Verify that the results of the parse are consistent
  AD.verify();

  // Prepare to generate the result files: 
  AD.generateMatchLists();
  AD.identify_cisc_spill_instructions();
  AD.identify_short_branches();
  // Make sure every file starts with a copyright:
  AD.addSunCopyright(AD._HPP_file._fp);           // .hpp
  AD.addSunCopyright(AD._CPP_file._fp);           // .cpp
  AD.addSunCopyright(AD._CPP_CLONE_file._fp);     // .cpp
  AD.addSunCopyright(AD._CPP_EXPAND_file._fp);    // .cpp
  AD.addSunCopyright(AD._CPP_FORMAT_file._fp);    // .cpp
  AD.addSunCopyright(AD._CPP_GEN_file._fp);       // .cpp
  AD.addSunCopyright(AD._CPP_MISC_file._fp);      // .cpp
  AD.addSunCopyright(AD._CPP_PEEPHOLE_file._fp);  // .cpp
  AD.addSunCopyright(AD._CPP_PIPELINE_file._fp);  // .cpp
  // Make sure each .cpp file starts with include lines:
  // files declaring and defining generators for Mach* Objects (hpp,cpp)
  AD.machineDependentIncludes(AD._CPP_file);      // .cpp
  AD.machineDependentIncludes(AD._CPP_CLONE_file);     // .cpp
  AD.machineDependentIncludes(AD._CPP_EXPAND_file);    // .cpp
  AD.machineDependentIncludes(AD._CPP_FORMAT_file);    // .cpp
  AD.machineDependentIncludes(AD._CPP_GEN_file);       // .cpp
  AD.machineDependentIncludes(AD._CPP_MISC_file);      // .cpp
  AD.machineDependentIncludes(AD._CPP_PEEPHOLE_file);  // .cpp
  AD.machineDependentIncludes(AD._CPP_PIPELINE_file);  // .cpp
  // Generate the result files: 
  // enumerations, class definitions, object generators, and the DFA
  // file containing enumeration of machine operands & instructions (hpp)
  AD.addPreHeaderBlocks(AD._HPP_file._fp);        // .hpp
  AD.buildMachOperEnum(AD._HPP_file._fp);         // .hpp
  AD.buildMachOpcodesEnum(AD._HPP_file._fp);      // .hpp
  AD.buildMachRegisterNumbers(AD._VM_file._fp);   // VM file
  AD.buildMachRegisterEncodes(AD._HPP_file._fp);  // .hpp file
  AD.declareRegSizes(AD._HPP_file._fp);           // .hpp
  AD.build_pipeline_enums(AD._HPP_file._fp);      // .hpp
  // output definition of class "State"
  AD.defineStateClass(AD._HPP_file._fp);          // .hpp
  // file declaring the Mach* classes derived from MachOper and MachNode
  AD.declareClasses(AD._HPP_file._fp);
  // declare and define maps: in the .hpp and .cpp files respectively
  AD.addSourceBlocks(AD._CPP_file._fp);           // .cpp
  AD.addHeaderBlocks(AD._HPP_file._fp);           // .hpp
  AD.buildReduceMaps(AD._HPP_file._fp, AD._CPP_file._fp);
  AD.build_rematerialize_map(AD._HPP_file._fp, AD._CPP_file._fp);
  AD.buildMustCloneMap(AD._HPP_file._fp, AD._CPP_file._fp);
  // build CISC_spilling oracle and MachNode::cisc_spill() methods
  AD.build_cisc_spill_instructions(AD._HPP_file._fp, AD._CPP_file._fp);
  // define methods for machine dependent State, MachOper, and MachNode classes
  AD.defineClasses(AD._CPP_file._fp);
  AD.buildMachOperGenerator(AD._CPP_GEN_file._fp);// .cpp
  AD.buildMachNodeGenerator(AD._CPP_GEN_file._fp);// .cpp
  // define methods for machine dependent instruction matching
  AD.buildInstructMatchChecks(AD._CPP_file._fp);  // .cpp
  // define methods for machine dependent frame management
  AD.buildFrameMethods(AD._CPP_file._fp);         // .cpp

  // do this last:
  AD.addPreprocessorChecks(AD._CPP_file._fp);     // .cpp
  AD.addPreprocessorChecks(AD._CPP_CLONE_file._fp);     // .cpp
  AD.addPreprocessorChecks(AD._CPP_EXPAND_file._fp);    // .cpp
  AD.addPreprocessorChecks(AD._CPP_FORMAT_file._fp);    // .cpp
  AD.addPreprocessorChecks(AD._CPP_GEN_file._fp);       // .cpp
  AD.addPreprocessorChecks(AD._CPP_MISC_file._fp);      // .cpp
  AD.addPreprocessorChecks(AD._CPP_PEEPHOLE_file._fp);  // .cpp
  AD.addPreprocessorChecks(AD._CPP_PIPELINE_file._fp);  // .cpp

  // define the finite automata that selects lowest cost production
  AD.addSunCopyright(AD._DFA_file._fp);           // .cpp
  AD.machineDependentIncludes(AD._DFA_file);      // .cpp
  AD.buildDFA(AD._DFA_file._fp);

  AD.close_files(0);               // Close all input/output files

  // Final printout and statistics
  // cout << program;

  if( AD._dfa_debug & 2 ) {    // For higher debug settings, print timing info
    //    Timer t_stop;
    //    Timer t_total = t_stop - t_start; // Total running time
    //    cerr << "\n---Architecture Description Totals---\n";
    //    cerr << ", Total lines: " << TotalLines;
    //    float l = TotalLines;
    //    cerr << "\nTotal Compilation Time: " << t_total << "\n";
    //    float ft = (float)t_total;
    //    if( ft > 0.0 ) fprintf(stderr,"Lines/sec: %#5.2f\n", l/ft);
  }
  return (AD._syntax_errs + AD._semantic_errs + AD._internal_errs); // Bye Bye!!
}

//------------------------------usage------------------------------------------
static void usage(ArchDesc& AD)
{
  printf("Architecture Description Language Compiler\n\n");
  printf("Usage: adl [-doqw] [-Dflag[=def]] [-Uflag] [-cFILENAME] [-hFILENAME] [-aDFAFILE] ADLFILE\n");
  printf(" d  produce DFA debugging info\n");
  printf(" o  no output produced, syntax and semantic checking only\n");
  printf(" q  quiet mode, supresses all non-essential messages\n"); 
  printf(" w  suppress warning messages\n");
  printf(" c  specify CPP file name (default: %s)\n", AD._CPP_file._name);
  printf(" h  specify HPP file name (default: %s)\n", AD._HPP_file._name);
  printf(" a  specify DFA output file name\n");
  printf("\n");
}

//------------------------------open_file------------------------------------
int ArchDesc::open_file(bool required, ADLFILE & ADF, const char *action)
{
  if (required &&
      (ADF._fp = fopen(ADF._name, action)) == NULL) {
    printf("ERROR: Cannot open file for %s: %s\n", action, ADF._name);
    close_files(1);
    return 0;
  }
  return 1;
}

//------------------------------open_files-------------------------------------
int ArchDesc::open_files(void)
{
  if (_ADL_file._name == NULL)
  { printf("ERROR: No ADL input file specified\n"); return 0; }

  if (!open_file(true       , _ADL_file, "r"))          { return 0; }
  if (!open_file(!_no_output, _DFA_file, "w"))          { return 0; }
  if (!open_file(!_no_output, _HPP_file, "w"))          { return 0; }
  if (!open_file(!_no_output, _CPP_file, "w"))          { return 0; }
  if (!open_file(!_no_output, _CPP_CLONE_file, "w"))    { return 0; }
  if (!open_file(!_no_output, _CPP_EXPAND_file, "w"))   { return 0; }
  if (!open_file(!_no_output, _CPP_FORMAT_file, "w"))   { return 0; }
  if (!open_file(!_no_output, _CPP_GEN_file, "w"))      { return 0; }
  if (!open_file(!_no_output, _CPP_MISC_file, "w"))     { return 0; }
  if (!open_file(!_no_output, _CPP_PEEPHOLE_file, "w")) { return 0; }
  if (!open_file(!_no_output, _CPP_PIPELINE_file, "w")) { return 0; }
  if (!open_file(!_no_output, _VM_file , "w"))          { return 0; }
  if (!open_file(_dfa_debug , _bug_file, "w"))          { return 0; }

  return 1;
}

//------------------------------close_file------------------------------------
void ArchDesc::close_file(int delete_out, ADLFILE& ADF)
{
  if (ADF._fp) {
    fclose(ADF._fp);
    if (delete_out) remove(ADF._name);
  }
}

//------------------------------close_files------------------------------------
void ArchDesc::close_files(int delete_out)
{
  if (_ADL_file._fp) fclose(_ADL_file._fp);

  close_file(delete_out, _CPP_file);
  close_file(delete_out, _CPP_CLONE_file);
  close_file(delete_out, _CPP_EXPAND_file);
  close_file(delete_out, _CPP_FORMAT_file);
  close_file(delete_out, _CPP_GEN_file);
  close_file(delete_out, _CPP_MISC_file);
  close_file(delete_out, _CPP_PEEPHOLE_file);
  close_file(delete_out, _CPP_PIPELINE_file);
  close_file(delete_out, _HPP_file);
  close_file(delete_out, _DFA_file);
  close_file(delete_out, _bug_file);

  if (!_quiet_mode) {
    printf("\n");
    if (_no_output || delete_out) {
      if (_ADL_file._name) printf("%s: ", _ADL_file._name);
      printf("No output produced");
    }
    else {
      if (_ADL_file._name) printf("%s --> ", _ADL_file._name);
      printf("%s, %s, %s, %s, %s, %s, %s, %s, %s", 
             _CPP_file._name, 
             _CPP_CLONE_file._name, 
             _CPP_EXPAND_file._name, 
             _CPP_FORMAT_file._name, 
             _CPP_GEN_file._name, 
             _CPP_MISC_file._name, 
             _CPP_PEEPHOLE_file._name, 
             _CPP_PIPELINE_file._name, 
             _HPP_file._name, _DFA_file._name);
    }
    printf("\n");
  }
}

//------------------------------strip_ext--------------------------------------
static char *strip_ext(char *fname)
{
  char *ep;
   
  if (fname) {
    ep = fname + strlen(fname) - 1; // start at last character and look for '.'
    while (ep >= fname && *ep != '.') --ep;
    if (*ep == '.')	*ep = '\0'; // truncate string at '.' 
  }
  return fname;
}

//------------------------------strip_path_and_ext------------------------------
static char *strip_path_and_ext(char *fname)
{
  char *ep;
  char *sp;
   
  if (fname) {
    for (sp = fname; *sp; sp++)
      if (*sp == '/')  fname = sp+1;
    ep = fname;                    // start at first character and look for '.'
    while (ep <= (fname + strlen(fname) - 1) && *ep != '.') ep++;
    if (*ep == '.')	*ep = '\0'; // truncate string at '.' 
  }
  return fname;
}

//------------------------------base_plus_suffix-------------------------------
// New concatenated string 
static char *base_plus_suffix(const char* base, const char *suffix)
{
  int len = (int)strlen(base) + (int)strlen(suffix) + 1;

  char* fname = new char[len];
  sprintf(fname,"%s%s",base,suffix);
  return fname;
}


//------------------------------prefix_plus_base_plus_suffix-------------------
// New concatenated string 
static char *prefix_plus_base_plus_suffix(const char* prefix, const char* base, const char *suffix)
{
  int len = (int)strlen(prefix) + (int)strlen(base) + (int)strlen(suffix) + 1;

  char* fname = new char[len];
  sprintf(fname,"%s%s%s",prefix,base,suffix);
  return fname;
}


void *operator new( size_t size, int, const char *, int ) {
  return ::operator new( size );
}
