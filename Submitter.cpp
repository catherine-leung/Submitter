#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "Vals.h"
#include "SubVals.h"
#include "Command.h"
#include "Submitter.h"
#include "Date.h"
#include "Line.h"
#include "colors.h"

using namespace std;
namespace sict {
  Submitter::Submitter(int argc, char** argv) :
    m_home(argv[0]),
    m_cls("clear"){
    m_argc = argc;
    m_argv = argv;
    m_accommExtension = 0;
    m_skipNewlines = m_skipSpaces = false;
    if (m_argc >= 2) m_configFileName = argv[1];
    size_t last = m_home.find_last_of('/');
    if (last != string::npos) {
      m_home = m_home.substr(0, last + 1);
    }
#ifdef SICT_DEBUG
    m_home = "";
#endif
    m_ok2submit = true;
    m_late = false;
  }
  void Submitter::clrscr()const {
#ifdef SICT_DEBUG
    system("cls");
#else
    m_cls.run();
#endif

  }
  bool Submitter::yes()const {
    char res = cin.get();
    cin.ignore(1000, '\n');
    while (res != 'Y' && res != 'y' && res != 'N' && res != 'n') {
      cout << col_red << "Only Y or N are acceptable: " << col_end;
      res = cin.get();
      cin.ignore(1000, '\n');
    }
    return res == 'Y' || res == 'y';
  }
  void Submitter::setSubmitterDir() {
    bool done = false;
    Vals V('|');
    string  path = m_home + SUB_CFG_FILE;
    ifstream file(path.c_str());
    while (file && !done) {
      file >> V;  // read a line of config file
      if (V.size() > 0) { // if any values read
        done = true;
        if (V[0] == "dir") {
          m_submitterDir = m_home + V[1];
        }
      }
    }
    file.close();
    if (m_submitterDir.length() == 0) m_submitterDir = m_home + SUB_DEF_DIR;
  }
  bool Submitter::getAccommodation() {
    bool ok = true;
    bool firstTitle = true;
    int extension;
    string accSubject;
    Vals V('|');
    std::string fname(m_submitterDir + "/" + SUB_ACC_FILE);
    ifstream file(fname.c_str());
    while (file) {
      file >> V;
      if (V.size() > 1) {
        m_accom.add(V[0], Vals(V[1], ','));
      }
    }
    file.close();
    if (ok) {
      int i;
      /*for (i = 0; i < m_accom.size(); i++) {
        cout << m_accom.value(i)[0] << endl;
      }*/
      for (i = 0; i < m_accom.size(); i++) {
        if (!m_asVals["subject_code"].compare(0, m_accom[i].size(), m_accom[i])) { // subject matches
          if (m_accom.values(i).size() > 3) { // if assignment name and extension days available with at least one username
            if (assignmentMatch(m_accom.values(i)[0])) {
              if (sscanf(m_accom.values(i)[2].c_str(), "%d", &extension) == 1) {
                for (int j = 3; j < m_accom.values(i).size(); j++) {
                  if (m_user == m_accom.values(i)[j]) {
                    m_accommExtension += extension;
                    if (!firstTitle) m_accommTitle += " & ";
                    else firstTitle = false;
                    m_accommTitle += m_accom.values(i)[1];
                  }
                }
              }
              else {
                cout << col_red << "Error# 26.1 Bad accommodation file entry (extension value) format for "
                  << m_accom[i] << col_end << endl;
              }
            }
          }
          else {
            cout << col_red << "Error# 26 Bad accommodation file entry format for "
              << m_accom[i] << col_end << endl;
          }
        }
      }
    }
    if (m_accommTitle.size() > 0) m_accommTitle += " ";
    return ok;
  }
  bool Submitter::assignmentMatch(string assignmentName)const {
    int i;
    bool match = true;
    for (i = 0;match && i < assignmentName.size(); i++) {
      if (assignmentName[i] != '?') {
        match = m_configFileName[i] == assignmentName[i];
      }
    }
    return match;
  }
  bool Submitter::getAssignmentValues() {
    bool ok = false;
    Vals V('|');
    std::string fname(m_submitterDir + "/" + m_configFileName);
    fname += ".cfg";
    ifstream file(fname.c_str());
    while (file) {
      file >> V;
      if (V.size() > 1) {
        ok = true;
        m_asVals.add(V[0], Vals(V[1], ','));
      }
    }
    file.close();
    if (ok) {
      ok = getAccommodation();
    }
    return ok;
  }
  bool Submitter::copyProfFiles() {
    bool ret = true;
    if (m_asVals.exist("copy_files")) {
      ret = m_asVals["copy_files"].size() > 0;
      int i;
      for (i = 0; ret && i < m_asVals["copy_files"].size(); i++) {
        Command cmd("cp ");
        cmd += (m_submitterDir + "/" + m_asVals["copy_files"][i] + " .");
        // cout << cmd << endl; // to show or not to show!
        ret = (cmd.run() == 0);
      }
    }
    return ret;
  }
  bool Submitter::filesExist() {
    Vals& files = m_asVals["assess_files"];
    bool ret = files.size() > 0;
    ifstream file;
    int i;
    for (i = 0; ret && i < files.size(); i++) {
      file.open(files[i]);
      if (!file) {
        m_ok2submit = ret = false;
        cout << files[i] << ", is missing!" << endl;
      }
      else {
        file.close();
      }
      file.clear();
    }
    return ret;
  }

  void Submitter::diff(ostream& os, const char* stdnt, const char* prof, int line, int stIdx, int profIdx) {
    os << "In line number " << line << " of your output:" << endl;
    os << "The output should be:" << endl << Line(prof,profIdx) << endl;
    os << "But your output is: " << endl << Line(stdnt,stIdx) << endl;
    os << endl << "Unmatched character details:" << endl;
    os << "The character in column " << (stIdx + 1)
      << " is supposed to be:" << endl << "  [" << Line(prof)[profIdx] << "] ASCII code(" << int(prof[profIdx]) << ")" << endl
      << "but you printed" << endl << "  [" << Line(stdnt)[stIdx] << "] ASCII code(" << int(stdnt[stIdx]) << ")" << endl << endl;
   }
  bool Submitter::lineCompare(const char *std, const char* prof, int& stIdx, int& profIdx) {
    int pi = -1;
    int si = -1;
    bool same = true;
    do {
      pi++;
      si++;
      if (m_skipSpaces) {
        while (prof[pi] && isSpace(prof[pi])) {
          pi++;
        }
        while (std[si] && isSpace(std[si])) {
          si++;
        }
      }
      same = prof[pi] == std[si];
    } while (same && prof[pi] && std[si]);
    if (!same) {
      stIdx = si;
      profIdx = pi;
    }
    return same;
  }
 
  bool Submitter::compare(const char* stdnt, const char* prof, int line) {
    int pi = -1;  // prof unmatched index
    int si = -1;  // student unmatched index
    bool ok = lineCompare(stdnt,prof, si, pi);
    if (!ok) {
      diff(cout, stdnt, prof, line,si, pi);
      ofstream diffFile("diff.txt");
      diff(diffFile, stdnt, prof, line,si, pi);
      diffFile.close();
    }
    return ok;
  }
  bool Submitter::removeBS(const char* filename) {
    bool good = true;
    fstream file(filename, ios::in);
    if (!file) {
      cout << "Error #17.1: could not open " << filename << endl;
      good = false;
    }
    else {
      int size;
      file.seekg((ios::off_type)0, ios::end);
      size = int(file.tellg());
      char* buf = new char[size + 1];
      file.seekg((ios::off_type)0);
      char ch;
      int i = 0;
      while (!file.fail()) {
        ch = file.get();
        if (!file.fail()) {
          if (ch == '\b') {
            if (i > 0) i--;
          }
          else {
            buf[i++] = ch;
          }
        }
      }
      buf[i] = 0;
      file.close();
      file.open(filename, ios::out);
      if (!file) {
        cout << "Error #17.2: could not open " << filename << " for output" << endl;
        good = false;
      }
      else {
        i = 0;
        while (buf[i]) {
          file.put(buf[i++]);
        }
        file.close();
      }
      delete[] buf;
    }
    return good;
  }

  bool Submitter::skipLine(int lineNo) {
    int skipNum = m_asVals["comp_range"].size() - 2;
    bool skip = false;
    int curLine;
    for (int i = 0; !skip && i < skipNum; i++) {
      if (sscanf(m_asVals["comp_range"][i + 2].c_str(), "%d", &curLine) == 1 && curLine == lineNo) {
        skip = true;
#ifdef SICT_DEBUG
        cout << "Skipping line " << curLine << endl;
#endif
      }
    }
    return skip;
  }
  bool Submitter::isEmptyLine(const char* line) {
    int i= 0;
    bool isEmpty = true;
    while (isEmpty &&  line[i]) {
      if (!isSpace(line[i])) {
        isEmpty = false;
      }
      i++;
    }
    return isEmpty;
  }
  bool Submitter::compareOutputs(int from, int to) {
    char sstr[4096], pstr[4096];
    bool good = true;
    //bool longFile = false;
    int pline = 0;
    int sline = 0;
    ifstream stfile(m_asVals["output_file"][0].c_str());
    ifstream prfile(m_asVals["correct_output"][0].c_str());
    if (!stfile) {
      cout << "Error #17: could not open " << m_asVals["output_file"][0] << endl;
      good = false;
    }
    if (!prfile) {
      cout << "Error #17: could not open " << m_asVals["correct_output"][0] << endl;
      good = false;
    }
    while (pline < to && good && stfile && prfile) {
      sstr[0] = pstr[0] = 0;
      do {
        sline++;
        stfile.getline(sstr, 4095, '\n');
      } while (m_skipNewlines && isEmptyLine(sstr) && stfile);
      do {
        pline++;
        prfile.getline(pstr, 4095, '\n');
      } while (m_skipNewlines && isEmptyLine(pstr) && prfile);
      if (!skipLine(pline) && pline >= from && pline <= to) {
        m_ok2submit = good = compare(sstr, pstr, pline);
      }
    }
    if (pline < from) {
      m_ok2submit = good = false;
      cout << "Your output file is too short or empty!" << endl;
    }
    /*if (m_skipNewlines && good) {
      do {
        sline++;
        stfile.getline(sstr, 4095, '\n');
      } while (m_skipNewlines && isEmptyLine(sstr) && stfile);
      if (!isEmptyLine(sstr)) {
        m_ok2submit = good = false;
        longFile = true;
      }
    }
    else if(good){
      stfile.getline(sstr, 4095, '\n');
      if (stfile) {
        m_ok2submit = good = false;
        longFile = true;
      }
    }
    if (longFile) {
      cout << endl << col_red << "Your output file is too long!" << col_end << endl;
      cout << "the following data found in your ouput where end of file was expected." << endl;
      cout << Line(sstr, 0) << endl;
      cout << "[" << Line(sstr)[0] << "] ASCII code(" << int(sstr[0]) << ")" << endl << endl;
    }*/
    return good;
  }
  int Submitter::compile() {
    int bad = 0;
    int i = 0;
    int errcode = 0;
#ifdef SICT_DEBUG
    return 0;
#endif // SICT_DEBUG

    if (m_asVals.exist("compile_command")) {
      Command compile(m_asVals["compile_command"][1]);
      if (m_asVals.exist("compile_files")) {
        for (i = 0; i < m_asVals["compile_files"].size(); i++) {
          compile += (" " + m_asVals["compile_files"][i]);
        }
        if (m_asVals.exist("err_file")) {
          compile += (" 2> " + m_asVals["err_file"][0]);
          cout << "Compiling:" << endl;
          cout << compile << endl << endl
            << "Compile result:" << endl;
          if ((errcode = compile.run()) != 0) {
            cout << "You have compilation errors. Please open \"" << m_asVals["err_file"][0] << "\" to veiw" << endl
              << "and correct them." << endl << "Submission aborted! (code: " << errcode << ")" << endl;
            m_ok2submit = false;
            bad = 9;
          }

          if (!bad && m_asVals["allow_warning"][0] != "yes") {
            if (Command("grep warning " + m_asVals["err_file"][0] + ">/dev/null").run() == 0) {
              cout << "You have compilation warnings. Please open \"" << m_asVals["err_file"][0] << "\" to veiw" << endl
                << "and correct them." << endl << "Submission aborted!" << endl;
              bad = 10;
              m_ok2submit = false;
            }
          }
        }
        else {
          cout << "Error #8: error log filename not specified!" << endl
            << "Please report this to your professor." << endl;
          bad = 8;
        }
      }
      else {
        cout << "Error #7: file names to be compiled not specified!" << endl
          << "Please report this to your professor." << endl;
        bad = 7;
      }
    }
    else {
      cout << "Error #6: Compile command not found!" << endl
        << "Please report this to your professor." << endl;
      bad = 6;
    }
#ifdef SICT_DEBUG
    bad = 0;
    m_ok2submit = true;
#endif // SICT_DEBUG

    return bad;
  }
  int Submitter::execute() {
    int bad = 0;
    if (!m_asVals.exist("exe_name")) {
      cout << "Error #11: executable filename not specified!" << endl
        << "Please report this to your professor." << endl;
      bad = 11;
    }
    else if (!m_asVals.exist("output_file")) {
      cout << "Error #12: output filename not specified!" << endl
        << "Please report this to your professor." << endl;
      bad = 12;
    }
    else {
      if (m_asVals["output_type"][0] == "script") {
        cout << endl << "READ THE FOLLOWING CAREFULLY!" << endl;
        cout << "I am about to execute the tester and capture the output in \"" << m_asVals["output_file"][0] << "\"" << endl;
        cout << "Please enter the values carefuly and exactly as instructed." << endl
          << "Press <ENTER> to start...";
        cin.ignore(1000, '\n');
        clrscr();
        Command("script " + m_asVals["output_file"][0] + " -c " + m_asVals["exe_name"][0]).run();
      }
      else {
        Command(m_asVals["exe_name"] + " >" + m_asVals["output_file"]).run();
      }
    }
    return bad;
  }
  int Submitter::checkOutput() {
    int bad = 0;
    int from = 0, to = 0;
    if (!removeBS(m_asVals["output_file"][0].c_str())) bad = 17;
    if (!bad && m_asVals.exist("comp_range")) {
      if (sscanf(m_asVals["comp_range"][0].c_str(), "%d", &from) == 1
        && sscanf(m_asVals["comp_range"][1].c_str(), "%d", &to) == 1) {
        if (m_asVals.exist("correct_output")) {
          if (Command("cp " + m_submitterDir + "/" + m_asVals["correct_output"][0] + " .").run() == 0) {
            if (!removeBS(m_asVals["correct_output"][0].c_str())) bad = 17;
            if (!compareOutputs(from, to)) {
              bad = 18;
              cout << "Outputs don't match. Submission aborted!" << endl << endl;
              cout << "To see exactly what is wrong, open the following two files in this" << endl
                << "directory and compare them: " << endl
                << "Your output file:    " << m_asVals["output_file"][0].c_str() << endl
                << "Correct output file: " << m_asVals["correct_output"][0].c_str() << endl << endl;
            }
            else {
              cout << "Success!... Outputs match." << endl;
            }
          }
          else {
            cout << "Error #15: could not access " << m_asVals["correct_output"][0] << "." << endl
              << "please report this to your professor!" << endl;
            bad = 15;
          }
        }
        else {
          cout << "Error #14: \"correct output\" is not specified!" << endl
            << "please report this to your professor!" << endl;
          bad = 14;
        }
      }
      else {
        cout << "Error #16: bad \"comparison range\" values!" << endl
          << "please report this to your professor!" << endl;
      }
    }
    else {
      cout << "Error #13: \"comparison range\" is not specified!" << endl
        << "please report this to your professor!" << endl;
      bad = 13;
    }
    return bad;
  }
  const char* Submitter::name() {
    return m_asVals["assessment_name"][0].c_str();
  }
  void Submitter::printCommandSyntaxHelp()const {
    cout << "Submission command format: " << endl;
    cout << "~prof_name.prof_lastname/submit DeliverableName [-submission option]<ENTER>" << endl;
    cout << "[-submission option] acceptable values: " << endl;
    cout << "  \"-skip_spaces\":" << endl;
    cout << "       Do the submission regardless of incorrect vertical spacing." << endl;
    cout << "       This option may attract penalty." << endl;
    cout << "  \"-skip_blank_lines\":" << endl;
    cout << "       Do the submission regardless of incorrect horizontal spacing." << endl;
    cout << "       This option may attract penalty." << endl;
  }
  bool Submitter::checkAndSetOption(string option) {
    bool ok = true;
    if (option != "-skip_spaces" && option != "-skip_blank_lines") {
      cout << "Unrecognized option: " << option << endl;
      printCommandSyntaxHelp();
      ok = false;
    }
    else if (option == "-skip_spaces") {
      m_skipSpaces = true;
    }
    else if (option == "-skip_blank_lines") {
      m_skipNewlines = true;
    }
    else {
      m_skipNewlines = m_skipSpaces = false;
    }
    return ok;
  }
  /*run returns:
  0 OK
  1 incorrect command line argument
  2 cannot open submission figuration file
  3 assessment name missing in assignment configuration file
  4 Student does not have all the files available in submission directory
  5 tester files missing in professor's directory or directory not accessible
  6 Complier command not specified in config file
  7 file names to be compiled are not specified in the assessment config file!
  8 the err_file value is not specified in the assessment config file
  9 compile error in student assignment
  10 compile warning in student assignment
  11 the exe_name value is not specified in the assessment config file
  12 the output_file value is not specified in the assessment config file
  13 the comp_range value is not specified in the assessment config file
  14 the correct_output value is not specified in the assessment config file
  15 failed to copy the correct output from submitter dir. (bad permission setting?)
  16 the comp_range, line number values in the config file could not be read as integers
  17 could not open the output files
  18 output comparison failed
  19 the linux mail command failed at the end
  20 bad due date format in config file
  21 bad due date and late title sequence in config file
  23 bad rejection date format in config file
  24 logged in from multiple connections
  25 Ip number of submission does not match the location set by the professor
  26 Bad accommodation file entry format 
  */

  int Submitter::run() {
    int bad = 0;
    int i = 0;
    clrscr();
#ifdef SICT_DEBUG
    cout << col_yellow << "DEBUGGING SUBMITTER" << endl;
    cout << "Comment debug defines in debug.h to turn off debugging......." << col_end << endl;
#endif
    cout << col_grey << "Submitter (V" << SUBMITTER_VERSION << ")" << endl;
    cout << "by Fardad S. (Last update: " << SUBMITTER_DATE << ")" << endl
      << "===============================================================" << col_end << endl << endl;
    // if the command has valid format
    if (m_argc < 2 || m_argc > 4) {
      printCommandSyntaxHelp();
      bad = 1;
    }
    else {
      for (i = 2;!bad && i < m_argc; i++) {
        bad = !checkAndSetOption(m_argv[i]);
      }
    }
    if (!bad) {
      // from SUB_CFG_FILE file set the sibmitter directory
     /* bad = int(!getSubmitterValues()) * 2; see header file*/
      setSubmitterDir();
      // get the assignment specs and put it in AsVals
      bad = int(!getAssignmentValues()) * 2;
      bad && cout << "Error #2:" << endl << "Cannot submit delivarable: \"" << m_configFileName << "\" "
        << "for the submit command:" << endl << endl << "   ~profName.profLastname/submit [deliverable_name]<ENTER>" << endl << endl
        << "Make sure the deliverable_name \"" << m_configFileName << "\" is not misspelled." << endl
        << "If you continue to get this error message, include the submission" << endl
        << "command in an email and report it to your professor!" << endl;
    }
    if (!bad && !(m_asVals.exist("skip_spaces") && m_asVals["skip_spaces"][0] == "yes") && m_skipSpaces) {
      cout << "Your professor does not allow the -skip_spaces option for this submission!" << endl;
      bad = 1;
    }
    if (!bad && !(m_asVals.exist("skip_blank_lines") && m_asVals["skip_blank_lines"][0] == "yes") && m_skipNewlines) {
      cout << "Your professor does not allow the -skip_blank_lines option for this submission!" << endl;
      bad = 1;
    }
    
    if (m_asVals.exist("allowed_ips")) {
      bool outside = false;
      m_user.getIP();
      if (m_user.multipleLogins()) {
        cout << col_red << "Error# 24: You are logged in from multiple locations." << endl
          << "Please logoff from all other sessions and try again." << col_end << endl;
        bad = 24;
      }
      else {    
        bad = 25; // if one of the ips match, bad will be set to zero
        for (i = 0;bad && i < m_asVals["allowed_ips"].size(); i++) {
          if (!m_user.ip().compare(0, m_asVals["allowed_ips"][i].size(), m_asVals["allowed_ips"][i])) {
            bad = 0;
          }
        }
        if (bad) {
          cout << col_red << "Error# 25: You can not submit from this location." << endl
            << "If this seems to be a mistake, please notify your professor." << col_end << endl;
        }
      }
    }
    if (!bad && m_asVals.exist("rejection_date")) {
      std::stringstream ssReject;
      ssReject << m_asVals["rejection_date"][0];
      m_rejectionDate.read(ssReject);
      m_rejectionDate += m_accommExtension;
      if (m_rejectionDate.bad()) {
        cout << "Bad rejection date format in config file." << endl
          << "Please report this to your professor." << endl;
        bad = 22;
      }
      else if (m_now > m_rejectionDate) {
        cout << col_red << "*** Submission Rejected! ***" << col_end << endl
          << "The deadline for this " << m_accommTitle << "submission has passed(Due: " << m_rejectionDate << ")." << endl
          << "If you believe this to be an error, please discuss with your professor." << endl;
        m_ok2submit = false;
      }
    }
    if (!bad && m_ok2submit) {
      if (m_asVals.exist("announcement")) {
        char ch;
        ifstream anFile(m_submitterDir + "/" + m_asVals["announcement"][0]);
        if (anFile) cout << col_red << "Attention: " << endl;
        cout << col_cyan;
        while (anFile) {
          anFile.get(ch);
          if (anFile) cout.put(ch);
        }
        cout << col_red << "Press <ENTER> to continue..." << col_end;
        cin.ignore(1000, '\n');
        clrscr();
      }
    }

    if (m_ok2submit) {
      if (!bad) {
        // if Assignment name is set in the assignment spcs files
        if (m_asVals.exist("assessment_name")) {
          if (m_asVals.exist("submit_files")) {
            cout << "Submitting";
          }
          else {
            cout << "Testing";
          }
          if (m_skipSpaces || m_skipNewlines) {
            cout << " (Skipping";
            if (m_skipSpaces) cout << " spaces";
            if (m_skipSpaces && m_skipNewlines) cout << " and";
            if (m_skipNewlines) cout << " blank lines";
            cout << "): ";
          }
          cout << endl << name() << endl << endl;
        }
        else { // otherwise exit with error
          cout << "Error #3: \"assessment_name\" is not specified!" << endl
            << "please report this to your professor!" << endl;
          bad = 3;
        }
      }

      if (!bad) {
        // check to make sure all files to be submitted is in
        // the currect dir.
        // fileExist function already prints the name of missing files
        bad = int(!filesExist()) * 4;
        bad && cout << "Error #4: submission files missing." << endl;
        m_ok2submit = !bad;
      }
      if (!bad) {
        bad = int(!copyProfFiles()) * 5;
        bad && cout << "Error #5: Could not copy tester files!" << endl << "Please report this to your professor!" << endl;
      }


      if (!bad && m_asVals.exist("due_dates")) {
        int li = m_asVals["due_dates"].size();
        if (li % 2 == 0) {
          std::stringstream ssDue;
          cout << endl << "Checking "<< m_accommTitle <<"due date:" << endl;
        
          m_late = false;
          while (!m_late && !bad && li > 0) {
            ssDue.clear();
            ssDue.str(std::string());
            m_lateTitle = m_asVals["due_dates"][--li];
            ssDue << m_asVals["due_dates"][--li];
            m_dueDate.read(ssDue);
             if (m_dueDate.bad()) {
              cout << "Error #20: bad due date format in config file." << endl
                << "Please report this to your professor!" << endl;
              bad = 20;
            }
             m_dueDate += m_accommExtension;
             if (m_now > m_dueDate) {
              m_late = true;
            }
          }
        }
        else {
          cout << "Error #21: bad due date and late title sequence in config file." << endl
            << "Please report this to your professor!" << endl;
          bad = 20;
        }
        if (!bad) {
          if (m_late) {
            if (m_lateTitle.length() == 0) m_lateTitle.assign("LATE");
            cout << col_yellow <<  m_lateTitle << "!" << col_end << endl << endl;
          }
          else {
            cout << col_green <<  "ON TIME." << col_end << endl << endl;
          }
        }
      }



      if (!bad && m_asVals["compile"][0] == "yes") {
        if ((bad = compile()) == 0) {
          cout << "Success! no errors or warnings..." << endl;
        }
      }
      if (!bad && m_asVals["execute"][0] == "yes") {
        cout << endl << "Testing execution:";
        bad = execute();
      }
      if (!bad && m_asVals["check_output"][0] == "yes") {
        cout << endl << "Checking output:" << endl;
        bad = checkOutput();
      }

      if (!bad && m_ok2submit) {
        if (m_asVals.exist("submit_files")) {
          cout << endl << "Submission: " << endl;
          if (!bad && m_asVals.exist("due_dates")) {
            if (m_late) {
              cout << "*** This " << m_accommTitle << "submission is " << m_lateTitle << "; the due date was: " << m_dueDate << " ***" << endl;
            }
            else {
              cout << "On time "<<m_accommTitle<<"submission, due date: " << m_dueDate << endl << endl;
            }
          }
          cout << "Would you like to submit this demonstration of " << name() << "? (Y)es/(N)o: ";
          if (yes()) {
            if (submit(m_asVals["prof_email"][0])) {
              cout << "Thank you!, Your work is now submitted." << endl;
            }
            else {
              bad = 19;
              cout << "Error #19: email failed." << endl
                << "Please report this to your professor" << endl;
            }
            if (!bad) {
              if (!m_asVals.exist("CC_student") || m_asVals["CC_student"][0] == "yes") {
                if (submit(m_asVals["prof_email"][0], true)) {
                  cout << "Confirmation of the submission is sent to your \"myseneca.ca\" email." << endl;
                }
                else {
                  bad = 19;
                  cout << "Error #19: confirmation email failed." << endl
                    << "Please report this to your professor" << endl;
                }
              }
            }
            if (!bad && m_asVals["prof_email"].size() > 1) {
              cout << endl << "Would you like to submit a copy of this demonstration of " << name() << " to the TA for feedback? (Y)es/(N)o: ";
              if (yes()) {
                for (i = 1; i < signed(m_asVals["prof_email"].size()); i++) {
                  if (submit(m_asVals["prof_email"][i])) {
                    cout << "CC no " << i << " is sent to the TA for feedback." << endl;
                  }
                  else {
                    bad = 19;
                    cout << "Error #19: email CC failed." << endl
                      << "Please report this to your professor" << endl;
                  }
                }
              }
            }
          }
          else {
            cout << "Submission aborted by user!" << endl;
          }
        }
        else {
          cout << "Test Successful!" << endl;
        }
      }
    }
    return bad;
  }

  bool Submitter::submit(string& toEmail, bool Confirmation) {
    Command email("echo \"");
    bool include_output_in_email = false;
    email += name();
    if (m_late) {
      email += " ";
      email += m_lateTitle;
    }
    if (m_skipSpaces || m_skipNewlines) {
      email += " with bad";
      if (m_skipSpaces) email += " spacing";
      if (m_skipSpaces && m_skipNewlines) email += " and";
      if (m_skipNewlines) email += " newlines";
    }
    email += m_accommTitle;
    email += " submission";
    if (Confirmation) email += " confirmation";
    email += " by `whoami`. Executed from ";
    email += m_home;
    email += "\" | mail -s \"";
    email += m_asVals["subject_code"][0] + " - ";
    email += name();
    if (m_late) {
      email += " ";
      email += m_lateTitle;
    }
    if (m_skipSpaces || m_skipNewlines) {
      email += " with bad";
      if (m_skipSpaces) email += " spacing";
      if (m_skipSpaces && m_skipNewlines) email += " and";
      if (m_skipNewlines) email += " newlines";
    }
    email += " submission by `whoami`\" ";
    email += " -Sreplyto=`whoami`@myseneca.ca ";
    for (int i = 0; i < m_asVals["submit_files"].size(); i++) {
      email += " -a " + m_asVals["submit_files"][i];
      if (m_asVals.exist("output_file") && m_asVals["submit_files"][i] == m_asVals["output_file"][0]) {
        include_output_in_email = true;
      }
    }
    if (m_asVals.exist("output_file") && !include_output_in_email && (m_skipSpaces || m_skipNewlines) ) {
        email += " -a " + m_asVals["output_file"][0];
    }

    if (Confirmation) { // send email to student from prof and ingore the toEmail argument
      email += " `whoami`@myseneca.ca";
    }
    else {
      email += " " + toEmail;
    }
#ifdef SICT_DEBUGEMAIL
    cout << email << endl;
    return true;
#else
    return email.run() == 0;
#endif


    }
  }






