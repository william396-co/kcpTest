#include <random>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

/* random(start,end)*/
int random( int start, int end );

//random_default()
int random_default();

//random_string(len)
string random_string( int len );

template<typename T>
void knuth_random( std::vector<T> & vec )
{
    for ( size_t i = vec.size() - 1; i > 0; --i ) {
        std::swap( vec[i], vec[random( 0, 100 ) % ( i + 1 )] );
    }
}
