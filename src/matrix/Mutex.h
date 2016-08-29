/*******************************************************************
 ** Mutex.h - Declares a Mutex class that encapsulates Windows/Posix
 *            mutexes.
 *
 *  Copyright (C) 2002 Associated Universities, Inc. Washington DC, USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Correspondence concerning GBT software should be addressed as follows:
 *  GBT Operations
 *  National Radio Astronomy Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *  $Id: Mutex.h,v 1.4 2006/09/28 18:51:30 rcreager Exp $
 *
 *******************************************************************/

#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

/// Enapsulates a pthread mutex for mutual exclusion.
class Mutex
{
  public:

    virtual ~Mutex();
    Mutex();

    int unlock();
    int lock();

  protected:
    pthread_mutex_t mutex;
};

/// A template to add a mutex lock/unlock to an stl container, so
/// one can write constructs like:
///
///      Protected< vector<int> >pc;
///      pc.lock();
///      pc.push_back(10);
///      pc.unlock();
///
/// This can also be used with the ThreadLock template:
///      Protected< vector<int> >pc;
///      ThreadLock<decltype(pc)> thread_lock(pc);
///
template <typename Container>
class Protected : public Container
{
public:
    int lock()   { return mtx.lock(); }
    int unlock() { return mtx.unlock(); }
protected:
    Mutex mtx;
};



#endif
