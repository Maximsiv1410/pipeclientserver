
/*******************************************************************************/


#pragma once

// STL headers
#include <vector>
#include <exception>


namespace binary_buffer_space{

class c_bindata_bufer{
private:
	std::vector< char >	m_vcDataBufer;		// bufer representation
	int					m_ncurpos;			// current position
public:
	// construction / destruction
	c_bindata_bufer() : m_ncurpos( 0 )
	{}
	explicit c_bindata_bufer(size_t prealloc) : m_ncurpos( 0 )
    {
        m_vcDataBufer.reserve(prealloc);
    }
    c_bindata_bufer(const c_bindata_bufer &other) :
        m_vcDataBufer( other.m_vcDataBufer ),
        m_ncurpos( other.m_ncurpos )
    {
    }
    c_bindata_bufer(c_bindata_bufer &&other)
    {
        m_vcDataBufer.swap(other.m_vcDataBufer);
        m_ncurpos = other.m_ncurpos;
        //
        other.m_ncurpos = 0;
    }
	~c_bindata_bufer()
	{}
	// write data to bufer
	void write( void const *pcvData, unsigned long ulDataSize ) throw()
    {
        if( !ulDataSize ){
            //LOGDBG( _T( "c_bindata_bufer::write() - data size is zero" ) );
            return;
        }
        size_t newSize = m_ncurpos + ulDataSize;
        if (newSize > m_vcDataBufer.size()) {
            m_vcDataBufer.resize(newSize);
        }
        // write data
        ::memcpy( &m_vcDataBufer[ m_ncurpos ], pcvData, ulDataSize );
        m_ncurpos = static_cast<int>(newSize);
    }
	// read data
	bool read( void *pvData, unsigned long ulDataSize ) throw()
    {
        if( eof() ){
            //LOGDBG( _T( "c_bindata_bufer::read() failed - bufer size is less then reading position" ) );
            return false;
        }
        auto remainSize = m_vcDataBufer.size() - m_ncurpos;
        if (ulDataSize > remainSize) {
            ulDataSize = remainSize;
        }
        if (!ulDataSize) {
            return false;
        }
        ::memcpy(pvData, &m_vcDataBufer[m_ncurpos], ulDataSize);
        m_ncurpos += ulDataSize;
        return true;
    }
	// reset for reading once more from begin of bufer
	void reset() throw()
    {	
        m_ncurpos = 0;
    }
	// return bufer size
	unsigned long get_bufer_size()const throw()
    {
        return static_cast<unsigned long>(m_vcDataBufer.size());
    }
	// return generic pointer to bufer
	const void* get_bufer()const
    {
        checkEmptyBuffer();
        return &m_vcDataBufer[ 0 ];
    }
	void* get_bufer()
    {
        checkEmptyBuffer();
        return &m_vcDataBufer[ 0 ];
    }
	bool Attach( void const *pvBuffer, unsigned long cbBuffer )
    {
        if( !pvBuffer ){
            return false;
        }
        if( !cbBuffer ){
            m_vcDataBufer.clear();
            return true;
        }
        auto charr = reinterpret_cast<const char *>(pvBuffer);
        m_vcDataBufer.assign( charr, charr + cbBuffer );
        reset();
        return true;
    }
	// clear bufer
	void clear() throw()
    {
        m_ncurpos = 0;
        m_vcDataBufer.clear();
    }
	const void* get_current_point() const
    {
        checkEmptyBuffer();
        checkEof();
        return &m_vcDataBufer[ m_ncurpos ];
    }
	void skip( unsigned long cBytes )
    {
        checkEof();
        m_ncurpos += cBytes;
    }

	unsigned long get_current_pos() const throw()
    {
        return eof() ? m_vcDataBufer.size() : m_ncurpos;
    }

	bool eof() const throw()
    {
        return m_ncurpos >= static_cast<int>(m_vcDataBufer.size());
    }

	unsigned long get_avail_size() const throw()
    {
        return m_vcDataBufer.size() - get_current_pos();
    }

private:
	void checkEof() const
    {
        if( eof() ){
            throw std::out_of_range( "End of stream was reached" );
        }
    }
	void checkEmptyBuffer() const
    {
        if( m_vcDataBufer.empty() ){
            throw std::out_of_range( "Empty buffer" );
        }
    }
};// c_bindata_bufer

}// binary_buffer_space