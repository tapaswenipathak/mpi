// Copyright (C) 2005, 2006 Douglas Gregor.

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Message Passing Interface 1.1 -- Section 4.5. Gather
#ifndef BOOST_MPI_GATHER_HPP
#define BOOST_MPI_GATHER_HPP

#include <cassert>
#include <cstddef>
#include <numeric>
#include <boost/mpi/exception.hpp>
#include <boost/mpi/datatype.hpp>
#include <vector>
#include <boost/mpi/packed_oarchive.hpp>
#include <boost/mpi/packed_iarchive.hpp>
#include <boost/mpi/detail/point_to_point.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/detail/offsets.hpp>
#include <boost/assert.hpp>

namespace boost { namespace mpi {

namespace detail {
// We're gathering at the root for a type that has an associated MPI
// datatype, so we'll use MPI_Gather to do all of the work.
template<typename T>
void
gather_impl(const communicator& comm, const T* in_values, int n, 
            T* out_values, int root, mpl::true_)
{
  MPI_Datatype type = get_mpi_datatype<T>(*in_values);
  BOOST_MPI_CHECK_RESULT(MPI_Gather,
                         (const_cast<T*>(in_values), n, type,
                          out_values, n, type, root, comm));
}

// We're gathering from a non-root for a type that has an associated MPI
// datatype, so we'll use MPI_Gather to do all of the work.
template<typename T>
void
gather_impl(const communicator& comm, const T* in_values, int n, int root, 
            mpl::true_ is_mpi_type)
{
  assert(comm.rank() != root);
  gather_impl(comm, in_values, n, (T*)0, root, is_mpi_type);
}

// We're gathering at the root for a type that does not have an
// associated MPI datatype, so we'll need to serialize
// it.
template<typename T>
void
gather_impl(const communicator& comm, const T* in_values, int n, T* out_values, 
            int const* nslot, int const* nskip, int root, mpl::false_)
{
  int tag = environment::collectives_tag();
  int nproc = comm.size();
  // first, gather all size, these size can be different for
  // each process
  packed_oarchive oa(comm);
  for (int i = 0; i < n; ++i) {
    oa << in_values[i];
  }
  std::vector<int> oasizes(nproc);
  int oasize = oa.size();
  BOOST_MPI_CHECK_RESULT(MPI_Gather,
                         (&oasize, 1, MPI_INTEGER,
                          oasizes.data(), 1, MPI_INTEGER, 
                          root, MPI_Comm(comm)));
  // Gather the archives, which can be of different sizes, so
  // we need to use gatherv.
  // Everything is contiguous (in the transmitted archive), so 
  // the offsets can be deduced from the collected sizes.
  std::vector<int> offsets;
  if (comm.rank() == root) sizes2offsets(oasizes, offsets);
  packed_iarchive::buffer_type recv_buffer(std::accumulate(oasizes.begin(), oasizes.end(), 0));
  BOOST_MPI_CHECK_RESULT(MPI_Gatherv,
                         (const_cast<void*>(oa.address()), int(oa.size()), MPI_BYTE,
                          recv_buffer.data(), oasizes.data(), offsets.data(), MPI_BYTE, 
                          root, MPI_Comm(comm)));
  if (comm.rank() == root) {
    for (int src = 0; src < nproc; ++src) {
      // handle variadic case
      int nb = nslot ? nslot[src] : n;
      int skip = nskip ? nskip[src] : 0;
      std::advance(out_values, skip);
      if (src == root) {
        BOOST_ASSERT(nb == n);
        for (int i = 0; i < nb; ++i) {
          *out_values++ = *in_values++;
        }
      } else {
        packed_iarchive ia(comm,  recv_buffer, boost::archive::no_header, offsets[src]);
        for (int i = 0; i < nb; ++i) {
          ia >> *out_values++;
        }
      }
    }
  }
}

// We're gathering at a non-root for a type that does not have an
// associated MPI datatype, so we'll need to serialize
// it.
template<typename T>
void
gather_impl(const communicator& comm, const T* in_values, int n, T* out_values,int root, 
            mpl::false_ is_mpi_type)
{
  gather_impl(comm, in_values, n, out_values, (int const*)0, (int const*)0, root, is_mpi_type);
}
} // end namespace detail

template<typename T>
void
gather(const communicator& comm, const T& in_value, T* out_values, int root)
{
  BOOST_ASSERT(out_values || (comm.rank() != root));
  detail::gather_impl(comm, &in_value, 1, out_values, root, is_mpi_datatype<T>());
}

template<typename T>
void gather(const communicator& comm, const T& in_value, int root)
{
  BOOST_ASSERT(comm.rank() != root);
  detail::gather_impl(comm, &in_value, 1, (T*)0, root, is_mpi_datatype<T>());
}

template<typename T>
void
gather(const communicator& comm, const T& in_value, std::vector<T>& out_values,
       int root)
{
  if (comm.rank() == root) {
    out_values.resize(comm.size());
  }
  ::boost::mpi::gather(comm, in_value, out_values.data(), root);
}

template<typename T>
void
gather(const communicator& comm, const T* in_values, int n, T* out_values, 
       int root)
{
  detail::gather_impl(comm, in_values, n, out_values, root, 
                      is_mpi_datatype<T>());
}

template<typename T>
void
gather(const communicator& comm, const T* in_values, int n, 
       std::vector<T>& out_values, int root)
{
  if (comm.rank() == root) {
    out_values.resize(comm.size() * n);
  }
  ::boost::mpi::gather(comm, in_values, n, out_values.data(), root);
}

template<typename T>
void gather(const communicator& comm, const T* in_values, int n, int root)
{
  BOOST_ASSERT(comm.rank() != root);
  detail::gather_impl(comm, in_values, n, root, is_mpi_datatype<T>());
}


} } // end namespace boost::mpi

#endif // BOOST_MPI_GATHER_HPP
