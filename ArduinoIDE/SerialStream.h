/*----------------------------------------------------------------------------------------------------------------------
 *    BME:4920 - Biomedical Engineering Senior Design II 
 *    Team 13 | Remote Hand Exoskeleton
 *    Sullivan Bryant, Charley Dunham, Jared Gilliam
 *
 *
 *    This header file provides overloaded operators for the extern Serial class.
 *    It is overloaded similarly to std::cout << std::endl
 *    with << operator buffering the right-side value to the Print stream, separating types.
 *    Handy tool for continuous calls to Serial, and event provides a debugging option, where,
 *    if #PRINT_DEBUG is defined (in a header scoped within the scope of the project), calls to
 *    sr::debug will print verbose debugging statements. 
 *    
 *    Usage ex:
 *      sr::out << "Hello world" << sr::endl;
 *----------------------------------------------------------------------------------------------------------------------*/

#pragma once                                                    // Guards against multiple inclusions
#include <Arduino.h>                                            // For Print class */
#include <Print.h>                                              // brings in Arduino’s Print & __FlashStringHelper
#include <HardwareSerial.h>                                     // for Serial
/*
 * Encapsulation within 'sr' (serial) namespace.
 */
namespace sr {
    struct Stream {                                             // Small wrapper for any Print-derived object.
        Print& _p;                                              // Instance of the Print object
        explicit Stream(Print& p): _p(p) {}                     /* Explicit constructor requiring reference to a Print-derived object,
                                                                   using initializer-list syntax to instantiate public Print reference.  */
        template <typename T>                                   // Generic 'print this value' overload
        Stream& operator<<(const T& v) {
            _p.print(v);
            return *this;
        }
        Stream& operator<<(const __FlashStringHelper* v) {     // Overloaded '<<' operator for flash strings (F() macro)
            _p.print(v);
            return *this;
        }
        using Manip = Stream& (*)(Stream&);                    // Type alias for stream manipulators (functions taking/returning Stream&)
        Stream& operator<<(Manip m) {                          // Apply stream manipulator, i.e., sr::endl
            return m(*this);
        }
    }; // end struct Stream
    inline Stream out{ Serial };                            // Single instance bound to Serial.
    inline Stream& endl(Stream& s) {                           // endl manipulator declaration
        s._p.println();
        return s;
    }
    // Optional sr::debug to print verbose statements
    #ifdef DEBUG_ON
        inline Stream& debug{ Serial };
    #else
    struct NullStream {                                       // Define a null-sink
        template <typename T>
        NullStream& operator<<(const T&) {                    // Swallow printable types
            return *this;
        }
        NullStream& operator<<(Stream::Manip) {               // Swallow stream manips
            return *this;
        }
    };
    inline NullStream debug;                                  // No alias to Stream&, does nothing.
#endif
} // namespace sr
