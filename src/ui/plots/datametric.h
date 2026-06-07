#ifndef DATAMETRIC_H
#define DATAMETRIC_H

#include <qrect.h>
#include <qvector.h>
#include <qmutex.h>
#include <qreadwritelock.h>

// Thread-safe, append-only point series backing a live QwtSeriesData curve.
// DataPower / DataHeartRate / DataCadence / DataSpeed were four byte-identical
// singletons; they now share this CRTP base, which holds all the storage and
// locking logic. Each metric is a one-line `final` subclass (see the headers
// below) so existing call sites — DataPower::instance().append(...) etc. —
// keep working unchanged.
template <class Derived>
class DataMetricSingleton
{
public:
    static Derived &instance()
    {
        static Derived valueVector;
        return valueVector;
    }

    int size() const { return m_values.size(); }
    QPointF value( int index ) const { return m_values[index]; }
    QRectF boundingRect() const { return m_boundingRect; }

    void lock()   { m_lock.lockForRead(); }
    void unlock() { m_lock.unlock(); }

    void clearData() { m_values.clear(); }

    void append( const QPointF &sample )
    {
        m_mutex.lock();
        m_pendingValues += sample;

        const bool isLocked = m_lock.tryLockForWrite();
        if ( isLocked )
        {
            const int numValues = m_pendingValues.size();
            const QPointF *pendingValues = m_pendingValues.data();

            for ( int i = 0; i < numValues; i++ )
                appendLocked( pendingValues[i] );

            m_pendingValues.clear();
            m_lock.unlock();
        }

        m_mutex.unlock();
    }

    void clearStaleValues( double limit )
    {
        m_lock.lockForWrite();

        m_boundingRect = QRectF( 1.0, 1.0, -2.0, -2.0 ); // invalid

        const QVector<QPointF> values = m_values;
        m_values.clear();
        m_values.reserve( values.size() );

        int index;
        for ( index = values.size() - 1; index >= 0; index-- )
        {
            if ( values[index].x() < limit )
                break;
        }

        if ( index > 0 )
            appendLocked( values[index++] );

        while ( index < values.size() - 1 )
            appendLocked( values[index++] );

        m_lock.unlock();
    }

protected:
    DataMetricSingleton() : m_boundingRect( 1.0, 1.0, -2.0, -2.0 ) // invalid
    {
        m_values.reserve( 1000 );
    }
    virtual ~DataMetricSingleton() {}

private:
    DataMetricSingleton( const DataMetricSingleton & );
    DataMetricSingleton &operator=( const DataMetricSingleton & );

    // Append + grow the bounding rect. Caller must already hold the write lock.
    void appendLocked( const QPointF &sample )
    {
        m_values.append( sample );

        if ( m_boundingRect.width() < 0 || m_boundingRect.height() < 0 )
        {
            m_boundingRect.setRect( sample.x(), sample.y(), 0.0, 0.0 );
        }
        else
        {
            m_boundingRect.setRight( sample.x() );

            if ( sample.y() > m_boundingRect.bottom() )
                m_boundingRect.setBottom( sample.y() );

            if ( sample.y() < m_boundingRect.top() )
                m_boundingRect.setTop( sample.y() );
        }
    }

    QReadWriteLock m_lock;
    QVector<QPointF> m_values;
    QRectF m_boundingRect;

    QMutex m_mutex;                    // protects m_pendingValues
    QVector<QPointF> m_pendingValues;
};

#endif // DATAMETRIC_H
