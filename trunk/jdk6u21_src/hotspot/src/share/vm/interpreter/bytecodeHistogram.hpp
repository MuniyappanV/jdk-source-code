/*
 * Copyright (c) 1997, 2004, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

// BytecodeCounter counts the number of bytecodes executed

class BytecodeCounter: AllStatic {
 private:
  NOT_PRODUCT(static int   _counter_value;)
  NOT_PRODUCT(static jlong _reset_time;)

  friend class TemplateInterpreterGenerator;
  friend class         BytecodeInterpreter;

 public:
  // Initialization
  static void reset()                      PRODUCT_RETURN;

  // Counter info (all info since last reset)
  static int    counter_value()            PRODUCT_RETURN0 NOT_PRODUCT({ return _counter_value; });
  static double elapsed_time()             PRODUCT_RETURN0; // in seconds
  static double frequency()                PRODUCT_RETURN0; // bytecodes/seconds

  // Counter printing
  static void   print()                    PRODUCT_RETURN;
};


// BytecodeHistogram collects number of executions of bytecodes

class BytecodeHistogram: AllStatic {
 private:
  NOT_PRODUCT(static int _counters[Bytecodes::number_of_codes];)   // a counter for each bytecode

  friend class TemplateInterpreterGenerator;
  friend class         InterpreterGenerator;
  friend class         BytecodeInterpreter;

 public:
  // Initialization
  static void reset()                       PRODUCT_RETURN; // reset counters

  // Profile printing
  static void print(float cutoff = 0.01F)   PRODUCT_RETURN; // cutoff in percent
};


// BytecodePairHistogram collects number of executions of bytecode pairs.
// A bytecode pair is any sequence of two consequtive bytecodes.

class BytecodePairHistogram: AllStatic {
 public: // for SparcWorks
  enum Constants {
    log2_number_of_codes = 8,                         // use a power of 2 for faster addressing
    number_of_codes      = 1 << log2_number_of_codes, // must be no less than Bytecodes::number_of_codes
    number_of_pairs      = number_of_codes * number_of_codes
  };

 private:
  NOT_PRODUCT(static int  _index;)                      // new bytecode is shifted in - used to index into _counters
  NOT_PRODUCT(static int  _counters[number_of_pairs];)  // a counter for each pair

  friend class TemplateInterpreterGenerator;
  friend class         InterpreterGenerator;

 public:
  // Initialization
  static void reset()                       PRODUCT_RETURN;   // reset counters

  // Profile printing
  static void print(float cutoff = 0.01F)   PRODUCT_RETURN;   // cutoff in percent
};
