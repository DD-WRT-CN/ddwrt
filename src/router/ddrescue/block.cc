/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004-2014 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdint.h>

#include "block.h"


// Align pos to next boundary if size is big enough
//
void Block::align_pos( const int alignment )
  {
  if( alignment > 1 )
    {
    const int disp = alignment - ( pos_ % alignment );
    if( disp < alignment && disp < size_ )
      { pos_ += disp; size_ -= disp; }
    }
  }


// Align end to previous boundary if size is big enough
//
void Block::align_end( const int alignment )
  {
  if( alignment > 1 && size_ > 0 )
    {
    const long long new_end = end() - ( end() % alignment );
    if( pos_ < new_end ) size_ = new_end - pos_;
    }
  }


void Block::crop( const Block & b )
  {
  const long long p = std::max( pos_, b.pos_ );
  const long long s = std::max( 0LL, std::min( end(), b.end() ) - p );
  pos_ = p; size_ = s;
  }


bool Block::join( const Block & b )
  {
  if( this->follows( b ) ) pos_ = b.pos_;
  else if( !b.follows( *this ) ) return false;
  if( b.size_ > LLONG_MAX - end() )
    internal_error( "size overflow joining two Blocks." );
  size_ += b.size_;
  return true;
  }


// shift the border of two consecutive Blocks
void Block::shift( Block & b, const long long pos )
  {
  if( end() != b.pos_ || pos <= pos_ || pos >= b.end() )
    internal_error( "bad argument shifting the border of two Blocks." );
  b.size_ = b.end() - pos; b.pos_ = pos; size_ = pos - pos_;
  }


Block Block::split( long long pos, const int hardbs )
  {
  if( hardbs > 1 ) pos -= pos % hardbs;
  if( pos_ < pos && end() > pos )
    {
    const Block b( pos_, pos - pos_ );
    pos_ = pos; size_ -= b.size_;
    return b;
    }
  return Block( 0, 0 );
  }


Domain::Domain( const long long p, const long long s,
                const char * const logname, const bool loose )
  {
  const Block b( p, s );
  if( !logname || !logname[0] ) { block_vector.push_back( b ); return; }
  Logfile logfile( logname );
  if( !logfile.read_logfile( loose ? '?' : 0 ) )
    {
    char buf[80];
    snprintf( buf, sizeof buf,
              "Logfile '%s' does not exist or is not readable.", logname );
    show_error( buf );
    std::exit( 1 );
    }
  logfile.compact_sblock_vector();
  for( int i = 0; i < logfile.sblocks(); ++i )
    {
    const Sblock & sb = logfile.sblock( i );
    if( sb.status() == Sblock::finished ) block_vector.push_back( sb );
    }
  if( block_vector.empty() ) block_vector.push_back( Block( 0, 0 ) );
  else this->crop( b );
  }


void Domain::crop( const Block & b )
  {
  unsigned r = block_vector.size();
  while( r > 0 && b < block_vector[r-1] ) --r;
  if( r > 0 ) block_vector[r-1].crop( b );
  if( r <= 0 || block_vector[r-1].size() <= 0 )	// no block overlaps b
    { block_vector.clear(); block_vector.push_back( Block( 0, 0 ) ); return; }
  if( r < block_vector.size() )			// remove blocks beyond b
    block_vector.erase( block_vector.begin() + r, block_vector.end() );
  if( b.pos() <= 0 ) return;
  --r;		// block_vector[r] is now the last non-cropped-out block
  unsigned l = 0;
  while( l < r && block_vector[l] < b ) ++l;
  if( l < r ) block_vector[l].crop( b );	// crop block overlapping b
  if( l > 0 )					// remove blocks before b
    block_vector.erase( block_vector.begin(), block_vector.begin() + l );
  }
