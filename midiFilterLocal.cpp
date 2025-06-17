/**
    @file
    midi filter - patch midi filter
    doug hirlinger - doughirlinger@gmail.com

    @ingroup    examples
*/

#include "ext.h"
#include "ext_obex.h"
#include "ext_strings.h"
#include "ext_common.h"
#include "ext_systhread.h"
#include <array>
#include <vector>
using namespace std;


// a c++ class representing a number, and types for a vector of those numbers
class number {
private:
    long value;
public:
    number(long &newValue)
    {
        value = newValue;
    }

    void setValue(const long &newValue)
    {
        value = newValue;
    }

    void getValue(long &retrievedValue)
    {
        retrievedValue = value;
    }
};
typedef std::vector<number>        numberVector;
typedef numberVector::iterator    numberIterator;
typedef std::array<long, 2> numberArray;
typedef std::vector<numberArray> numberArrayVector;
typedef numberArrayVector::iterator numberArrayIterator;


// max object instance data
typedef struct _midiFilter {
    t_object            ob;
    numberVector        *m_localNotes;    // note: you must store this as a pointer and not directly as a member of the object's struct
    numberArrayVector   *m_reassignedNotes;
    bool                *m_isEmpty;
    void                *m_outlet;
    void                *m_outlet2;
    //t_systhread_mutex    m_mutex;
} t_midiFilter;


// prototypes
void    *midiFilter_new(t_symbol *s, long argc, t_atom *argv);
void    midiFilter_free(t_midiFilter *x);
void    midiFilter_assist(t_midiFilter *x, void *b, long m, long a, char *s);
void    midiFilter_printLocalNotes(t_midiFilter *x); //same as bang but for localNotes
void    midiFilter_count(t_midiFilter *x);
void    midiFilter_list(t_midiFilter *x, t_symbol *msg, long argc, t_atom *argv);
void    midiFilter_clear(t_midiFilter *x);
bool    midiFilter_contains(t_midiFilter *x, numberVector &collection, long targetValue);
bool    midiFilter_localMath(t_midiFilter *x, long value);
void    midiFilter_removeValue(t_midiFilter *x, numberVector &collection, long valueToRemove);
long    midiFilter_arrayContains(t_midiFilter *x, numberArrayVector &collection, long targetValue);
void    midiFilter_removeValuesFromArray(t_midiFilter *x, numberArrayVector &collection, long valueToRemove);
void    midiFilter_version();
void    midiFilter_printReassigned(t_midiFilter *x);


// globals
static t_class    *s_midiFilter_class = NULL;

/************************************************************************************/

void ext_main(void *r)
{
    t_class    *c = class_new("midiFilterLocal",
                           (method)midiFilter_new,
                           (method)midiFilter_free,
                           sizeof(t_midiFilter),
                           (method)NULL,
                           A_GIMME,
                           0);

    class_addmethod(c, (method)midiFilter_printLocalNotes,    "printLocalNotes",            0);
    class_addmethod(c, (method)midiFilter_list,    "list",            A_GIMME,0);
    class_addmethod(c, (method)midiFilter_clear,    "clear",        0);
    class_addmethod(c, (method)midiFilter_count,    "count",        0);
    class_addmethod(c, (method)midiFilter_assist,    "assist",        A_CANT, 0);
    class_addmethod(c, (method)stdinletinfo,    "inletinfo",    A_CANT, 0);
    class_addmethod(c, (method)midiFilter_contains, "int",      A_LONG, 0);
    class_addmethod(c, (method)midiFilter_localMath, "int", A_LONG, 0);
    class_addmethod(c, (method)midiFilter_removeValue, "int", A_LONG, 0);
    class_addmethod(c,(method)midiFilter_arrayContains, "int", A_LONG, 0);
    class_addmethod(c, (method)midiFilter_removeValuesFromArray, "int", A_LONG, 0);
    class_addmethod(c, (method)midiFilter_version, "version", 0);
    class_addmethod(c, (method)midiFilter_printReassigned, "printReassigned", 0);
    

    class_register(CLASS_BOX, c);
    s_midiFilter_class = c;
    
    post("midiFilterLocal object 1.0");
}


/************************************************************************************/
// Object Creation Method

void *midiFilter_new(t_symbol *s, long argc, t_atom *argv)
{
    t_midiFilter    *x;

    x = (t_midiFilter *)object_alloc(s_midiFilter_class);
    if (x) {
       //systhread_mutex_new(&x->m_mutex, 0);
        x->m_outlet2 = outlet_new(x, NULL);
        x->m_outlet = outlet_new(x, NULL);
        midiFilter_list(x, gensym("list"), argc, argv);
        x->m_localNotes = new numberVector;
        x->m_localNotes->reserve(22);
        x->m_reassignedNotes = new numberArrayVector;
        x->m_reassignedNotes->reserve(220);
        x->m_isEmpty = 0;
    }
    return(x);
}


void midiFilter_free(t_midiFilter *x)
{
    //systhread_mutex_free(x->m_mutex);
    delete x->m_localNotes;
}


/************************************************************************************/
// Methods bound to input/inlets

void midiFilter_assist(t_midiFilter *x, void *b, long msg, long arg, char *dst)
{
    if (msg==1)
        strcpy(dst, "input");
    else if (msg==2)
        strcpy(dst, "output");
}

void midiFilter_printLocalNotes(t_midiFilter *x)
{
    numberIterator iter, begin, end;
    int i = 0;
    long ac = 0;
    t_atom *av = NULL;
    long value;

//    systhread_mutex_lock(x->m_mutex);
    ac = x->m_localNotes->size();

    if (ac)
        av = new t_atom[ac];

    if (ac && av) {

        begin = x->m_localNotes->begin();
        end = x->m_localNotes->end();

        iter = begin;

        for (;;) {

            (*iter).getValue(value);
            atom_setlong(av+i, value);

            i++;
            iter++;

            if (iter == end)
                break;
        }
        //systhread_mutex_unlock(x->m_mutex);    // must unlock before calling _clear() or we will deadlock

//        DPOST("about to clear\n", ac);
//        midiFilter_clear(x);

        outlet_anything(x->m_outlet2, gensym("list"), ac, av); // don't want to call outlets in mutexes either

        delete[] av;
    }
    //else
       // systhread_mutex_unlock(x->m_mutex);
}


void midiFilter_count(t_midiFilter *x)
{
    outlet_int(x->m_outlet, x->m_localNotes->size());
}


void midiFilter_list(t_midiFilter *x, t_symbol *msg, long argc, t_atom *argv)
{
    //if there's an incoming list
    
    //systhread_mutex_lock(x->m_mutex);
    
    if (argc > 0) {
        
        long pitch = atom_getlong(argv);
        long velocity = atom_getlong(argv+1);
        
        
        
        }
        
    return;
}

//for midi coming from other channels via send and receive objects





void midiFilter_clear(t_midiFilter *x)
{
    //systhread_mutex_lock(x->m_mutex);
    x->m_localNotes->clear();
    x->m_reassignedNotes->clear();
    //systhread_mutex_unlock(x->m_mutex);
}

bool midiFilter_contains(t_midiFilter *x, numberVector &collection, long targetValue)
{
    numberIterator iter, begin, end;
    long value;
    bool found = false;

    // Lock the mutex to ensure thread safety
    //systhread_mutex_lock(x->m_mutex);

    // Check if the collection is not empty
    if (!collection.empty()) {
        begin = collection.begin();
        end = collection.end();
        iter = begin;

        // Iterate through the collection to find the target value
        for (;;)
        {
            (*iter).getValue(value);
            if (value == targetValue) {
                found = true;
                break;
            }

            iter++;
            if (iter == end)
                break;
        }
    }

    // Unlock the mutex
    //systhread_mutex_unlock(x->m_mutex);

    return found;
}

//returns -1 if no arrays[0] contain note else returns origin value before reassignment
long midiFilter_arrayContains(t_midiFilter *x, numberArrayVector &collection, long targetValue)
{
    long output = -1;
    
    if (!collection.empty()) {
        
        for (numberArrayIterator it = collection.begin(); it != collection.end(); ++it) {
            
            for(size_t j = 0; j < 2; ++j) {
                if ((*it)[0] == targetValue) {
                    output = (*it)[1];
                    return output;
                }
            }
            
        }
        
    }
    
    return output;
    
}

bool midiFilter_localMath(t_midiFilter *x, long value)
{
    numberIterator iter, begin, end;
    
    // true will signal pitch to conitue to output. false with drop the note
    
    bool response = true;
    long listValue;

    // Lock the mutex to ensure thread safety
    //systhread_mutex_lock(x->m_mutex);

    // Check if the collection is not empty
    if (!x->m_localNotes->empty()) {
        begin = x->m_localNotes->begin();
        end = x->m_localNotes->end();
        iter = begin;

        // Iterate through the collection to find the target value
        for (;;)
        {
                    // Use the iterator to get the current element's value
                    (*iter).getValue(listValue);

                    // Apply your custom math logic
                    if (value - listValue == 1 || value - listValue == 2 ||
                        value - listValue == -1 || value - listValue == -2) {
                        response = false;
                        break;
                    }

                    iter++;
                    if (iter == end)
                        break;
                }
    }

    // Unlock the mutex
    //systhread_mutex_unlock(x->m_mutex);

    return response;
}


void midiFilter_removeValue(t_midiFilter *x, numberVector &collection, long valueToRemove)
{
    // Lock the mutex to ensure thread safety
    //systhread_mutex_lock(x->m_mutex);

    numberIterator iter = collection.begin();

    // Iterate through the collection to find the element
    while (iter != collection.end())
    {
        long currentValue;
        (*iter).getValue(currentValue);

        // If the value matches, erase the element
        if (currentValue == valueToRemove) {
            iter = collection.erase(iter); // Erase returns the next iterator
            break; // Stop after removing the first matching element
        } else {
            ++iter; // Move to the next element
        }
    }

    // Unlock the mutex
    //systhread_mutex_unlock(x->m_mutex);
}

void midiFilter_removeValuesFromArray(t_midiFilter *x, numberArrayVector &collection, long valueToRemove)
{
    if (!collection.empty()) {
        for (numberArrayIterator it = collection.begin(); it != collection.end(); ) {
            if ((*it)[0] == valueToRemove) {
                it = collection.erase(it);  // Erase returns the next valid iterator
            } else {
                ++it;  // Only increment if no deletion occurs
            }
        }
    }
}


void midiFilter_printReassigned(t_midiFilter *x)
{
    numberArrayIterator iter, begin, end;
    int i = 0;
    long ac = 0;
    t_atom *av = NULL;

    ac = x->m_reassignedNotes->size() * 2; // Each entry in m_reassignedNotes contains two values

    if (ac)
        av = new t_atom[ac];

    if (ac && av) {
        begin = x->m_reassignedNotes->begin();
        end = x->m_reassignedNotes->end();
        iter = begin;

        for (; iter != end; ++iter) {
            atom_setlong(av + i, (*iter)[0]); // First value
            atom_setlong(av + i + 1, (*iter)[1]); // Second value
            i += 2;
        }

        outlet_anything(x->m_outlet2, gensym("list"), ac, av); // Send list to outlet

        delete[] av;
    }
}
    
void midiFilter_version()
{
    post("midiFilterLocal object 1.0");
}


