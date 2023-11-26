/*
  ==============================================================================

    LockFreeFifo.h
    Created: 16 Jun 2023 5:50:04pm
    Author:  Gavin

  ==============================================================================
*/

#pragma once

template<typename T>
class LockFreeFifo
{
public:
    LockFreeFifo(int size) : fifo(size), buffer(size) {}
    
    template<typename U>
    bool push(U&& item)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);
        
        if (size1 + size2 <= 0)
            return false;
        
        buffer[start1] = std::forward<U>(item);
        fifo.finishedWrite(1);
        
        return true;
    }
    
    T pop()
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead(1, start1, size1, start2, size2);
        
        if (size1 + size2 <= 0)
            return nullptr;
        
        T item = std::move(buffer[start1]);
        fifo.finishedRead(1);
        
        return item;
    }
    
    void clear()
    {
        fifo.reset();
    }
    
    int getNumItems() { return fifo.getNumReady(); }
    
private:
    juce::AbstractFifo fifo;
    std::vector<T> buffer;
};

/*
class A
{
public:
    A() : p(this)
    {
        
    }
    A* p;
};

class B
{
public:
    B() { }
    template<typename U>
    void setA(U&& item)
    {
        a = std::forward(item);
    }
    A a;
};

class C : A
{
    // extra stuff
}

int main()
{
    B b;
    b.setA(C());
    print(&(b.a));
    print(b.a.p);
}
*/
