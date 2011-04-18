/******************************************************************************
* Copyright (c) 2011, Michael P. Gerlek (mpg@flaxen.com)
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#include <libpc/drivers/las/Reader.hpp>

#include <libpc/drivers/las/Header.hpp>
#include <libpc/drivers/las/Iterator.hpp>
#include "LasHeaderReader.hpp"
#include <libpc/exceptions.hpp>
#include <libpc/PointBuffer.hpp>

namespace libpc { namespace drivers { namespace las {



LasReader::LasReader(const std::string& filename)
    : Stage()
    , m_filename(filename)
{
    std::istream* str = Utils::openFile(m_filename);

    LasHeaderReader lasHeaderReader(m_lasHeader, *str);
    lasHeaderReader.read( getSchemaRef() );

    this->setBounds(m_lasHeader.getBounds());
    this->setNumPoints(m_lasHeader.GetPointRecordsCount());

    Utils::closeFile(str);

    return;
}


const std::string& LasReader::getName() const
{
    static std::string name("Las Reader");
    return name;
}


const std::string& LasReader::getFileName() const
{
    return m_filename;
}


PointFormat LasReader::getPointFormat() const
{
    return m_lasHeader.getPointFormat();
}


boost::uint8_t LasReader::getVersionMajor() const
{
    return m_lasHeader.GetVersionMajor();
}


boost::uint8_t LasReader::getVersionMinor() const
{
    return m_lasHeader.GetVersionMinor();
}


boost::uint64_t LasReader::getPointDataOffset() const
{
    return m_lasHeader.GetDataOffset();
}


libpc::SequentialIterator* LasReader::createSequentialIterator() const
{
    return new SequentialIterator(*this);
}


libpc::RandomIterator* LasReader::createRandomIterator() const
{
    return new RandomIterator(*this);
}


boost::uint32_t LasReader::processBuffer(PointBuffer& data, std::istream& stream) const
{
    boost::uint32_t numPoints = data.getCapacity();

    const LasHeader& lasHeader = getLasHeader();
    const SchemaLayout& schemaLayout = data.getSchemaLayout();
    const Schema& schema = schemaLayout.getSchema();
    const PointFormat pointFormat = lasHeader.getPointFormat();

    const PointIndexes indexes(schema, pointFormat);

    const bool hasTime = Support::hasTime(pointFormat);
    const bool hasColor = Support::hasColor(pointFormat);
    const int pointByteCount = Support::getPointDataSize(pointFormat);

    assert(100 > pointByteCount);
    boost::uint8_t buf[100]; // set to something larger than the largest point format requires

    for (boost::uint32_t pointIndex=0; pointIndex<numPoints; pointIndex++)
    {
        Utils::read_n(buf, stream, pointByteCount);

        boost::uint8_t* p = buf;

        // always read the base fields
        {
            const boost::int32_t x = Utils::read_field<boost::int32_t>(p);
            const boost::int32_t y = Utils::read_field<boost::int32_t>(p);
            const boost::int32_t z = Utils::read_field<boost::int32_t>(p);
            const boost::uint16_t intensity = Utils::read_field<boost::uint16_t>(p);
            const boost::uint8_t flags = Utils::read_field<boost::uint8_t>(p);
            const boost::uint8_t classification = Utils::read_field<boost::uint8_t>(p);
            const boost::int8_t scanAngleRank = Utils::read_field<boost::int8_t>(p);
            const boost::uint8_t user = Utils::read_field<boost::uint8_t>(p);
            const boost::uint16_t pointSourceId = Utils::read_field<boost::uint16_t>(p);

            const boost::uint8_t returnNum = flags & 0x07;
            const boost::uint8_t numReturns = (flags >> 3) & 0x07;
            const boost::uint8_t scanDirFlag = (flags >> 6) & 0x01;
            const boost::uint8_t flight = (flags >> 7) & 0x01;

            data.setField<boost::uint32_t>(pointIndex, indexes.X, x);
            data.setField<boost::uint32_t>(pointIndex, indexes.Y, y);
            data.setField<boost::uint32_t>(pointIndex, indexes.Z, z);
            data.setField<boost::uint16_t>(pointIndex, indexes.Intensity, intensity);
            data.setField<boost::uint8_t>(pointIndex, indexes.ReturnNumber, returnNum);
            data.setField<boost::uint8_t>(pointIndex, indexes.NumberOfReturns, numReturns);
            data.setField<boost::uint8_t>(pointIndex, indexes.ScanDirectionFlag, scanDirFlag);
            data.setField<boost::uint8_t>(pointIndex, indexes.EdgeOfFlightLine, flight);
            data.setField<boost::uint8_t>(pointIndex, indexes.Classification, classification);
            data.setField<boost::int8_t>(pointIndex, indexes.ScanAngleRank, scanAngleRank);
            data.setField<boost::uint8_t>(pointIndex, indexes.UserData, user);
            data.setField<boost::uint16_t>(pointIndex, indexes.PointSourceId, pointSourceId);
        }

        if (hasTime)
        {
            const double time = Utils::read_field<double>(p);
            data.setField<double>(pointIndex, indexes.Time, time);
        }

        if (hasColor)
        {
            const boost::uint16_t red = Utils::read_field<boost::uint16_t>(p);
            const boost::uint16_t green = Utils::read_field<boost::uint16_t>(p);
            const boost::uint16_t blue = Utils::read_field<boost::uint16_t>(p);

            data.setField<boost::uint16_t>(pointIndex, indexes.Red, red);
            data.setField<boost::uint16_t>(pointIndex, indexes.Green, green);
            data.setField<boost::uint16_t>(pointIndex, indexes.Blue, blue);       
        }
        
        data.setNumPoints(pointIndex+1);
    }

    return numPoints;
}


} } } // namespaces
