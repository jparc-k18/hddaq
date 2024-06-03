// -*- C++ -*-

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <ctime>

#include "Recorder/recorderBookmarker.hh"

//______________________________________________________________________________
Bookmarker::Bookmarker(int run_number,
		       std::string dir_name)
  : m_run_number(run_number),
    m_filename(),
    m_bookmark(),
    m_index(0)
{
  if(dir_name.empty()){
    std::cerr << "#E dir name is empty" << std::endl;
    return;
  }
  if (dir_name[dir_name.size()-1]!='/')
    dir_name += "/";
  dir_name += "bookmark/";
  mkdir(dir_name.c_str(), 0755);
  std::stringstream ss;
  ss << "run" << std::setw(5) << std::setfill('0') << run_number
     << "_bookmark.dat";
  m_filename = dir_name + ss.str();

  std::ifstream bookmark_exists(m_filename.c_str());
  if (bookmark_exists.good())
    {
      bookmark_exists.close();
      throw std::runtime_error("Bookmark file already exists : "+m_filename);
    }
  m_bookmark.open(m_filename.c_str());
  if (m_bookmark.fail())
    std::cerr << "#E failed to open file: " << m_filename << std::endl;
  m_bookmark.write(reinterpret_cast<char*>(&m_index),
		   sizeof(unsigned long long));
}

//______________________________________________________________________________
Bookmarker::~Bookmarker()
{
  if (m_bookmark.is_open())
    {
      close();
    }
}

//______________________________________________________________________________
void
Bookmarker::operator+=(unsigned long long size)
{
  m_index += size;
  m_bookmark.write(reinterpret_cast<char*>(&m_index),
		   sizeof(unsigned long long));
  return;
}

//______________________________________________________________________________
void
Bookmarker::close()
{
  m_bookmark.flush();
  m_bookmark.close();
  ::chmod(m_filename.c_str(), S_IRUSR | S_IRGRP | S_IROTH);
  return;
}
