/*
 * @(#)Queue.java	1.13 03/12/19
 *
 * Copyright 2004 Sun Microsystems, Inc. All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */

package sun.misc;

import java.util.Enumeration;
import java.util.NoSuchElementException;

/**
 * Queue: implements a simple queue mechanism.  Allows for enumeration of the 
 * elements.
 *
 * @version 1.13, 12/19/03
 * @author Herb Jellinek
 */

public class Queue {

    int length = 0;

    QueueElement head = null;
    QueueElement tail = null;

    public Queue() {
    }

    /**
     * Enqueue an object.
     */
    public synchronized void enqueue(Object obj) {
	
	QueueElement newElt = new QueueElement(obj);

	if (head == null) {
	    head = newElt;
	    tail = newElt;
	    length = 1;
	} else {
	    newElt.next = head;
	    head.prev = newElt;
	    head = newElt;
	    length++;
	}
	notify();
    }

    /**
     * Dequeue the oldest object on the queue.  Will wait indefinitely.
     *
     * @return    the oldest object on the queue.
     * @exception java.lang.InterruptedException if another thread has
     *              interrupted this thread.
     */
    public Object dequeue() throws InterruptedException {
	return dequeue(0L);
    }

    /**
     * Dequeue the oldest object on the queue.
     * @param timeOut the number of milliseconds to wait for something 
     * to arrive.
     *
     * @return    the oldest object on the queue.
     * @exception java.lang.InterruptedException if another thread has
     *              interrupted this thread.
     */
    public synchronized Object dequeue(long timeOut)
        throws InterruptedException {
	
	while (tail == null) {
	    wait(timeOut);
	}
	QueueElement elt = tail;
	tail = elt.prev;
	if (tail == null) {
	    head = null;
	} else {
	    tail.next = null;
	}
	length--;
	return elt.obj;
    }

    /**
     * Is the queue empty?
     * @return true if the queue is empty.
     */
    public synchronized boolean isEmpty() {
	return (tail == null);
    }

    /**
     * Returns an enumeration of the elements in Last-In, First-Out
     * order. Use the Enumeration methods on the returned object to
     * fetch the elements sequentially.
     */
    public final synchronized Enumeration elements() {
	return new LIFOQueueEnumerator(this);
    }

    /**
     * Returns an enumeration of the elements in First-In, First-Out
     * order. Use the Enumeration methods on the returned object to
     * fetch the elements sequentially.
     */
    public final synchronized Enumeration reverseElements() {
	return new FIFOQueueEnumerator(this);
    }

    public synchronized void dump(String msg) {
	System.err.println(">> "+msg);
	System.err.println("["+length+" elt(s); head = "+
			   (head == null ? "null" : (head.obj)+"")+
			   " tail = "+(tail == null ? "null" : (tail.obj)+""));
	QueueElement cursor = head;
	QueueElement last = null;
	while (cursor != null) {
	    System.err.println("  "+cursor);
	    last = cursor;
	    cursor = cursor.next;
	}
	if (last != tail) {
	    System.err.println("  tail != last: "+tail+", "+last);
	}
	System.err.println("]");
    }
}

final class FIFOQueueEnumerator implements Enumeration {
    Queue queue;
    QueueElement cursor;

    FIFOQueueEnumerator(Queue q) {
	queue = q;
	cursor = q.tail;
    }

    public boolean hasMoreElements() {
	return (cursor != null);
    }

    public Object nextElement() {
	synchronized (queue) {
	    if (cursor != null) {
		QueueElement result = cursor;
		cursor = cursor.prev;
		return result.obj;
	    }
	}
	throw new NoSuchElementException("FIFOQueueEnumerator");
    }
}

final class LIFOQueueEnumerator implements Enumeration {
    Queue queue;
    QueueElement cursor;

    LIFOQueueEnumerator(Queue q) {
	queue = q;
	cursor = q.head;
    }

    public boolean hasMoreElements() {
	return (cursor != null);
    }

    public Object nextElement() {
	synchronized (queue) {
	    if (cursor != null) {
		QueueElement result = cursor;
		cursor = cursor.next;
		return result.obj;
	    }
	}
	throw new NoSuchElementException("LIFOQueueEnumerator");
    }
}

class QueueElement {
    QueueElement next = null;
    QueueElement prev = null;

    Object obj = null;

    QueueElement(Object obj) {
	this.obj = obj;
    }

    public String toString() {
	return "QueueElement[obj="+obj+(prev == null ? " null" : " prev")+
	    (next == null ? " null" : " next")+"]";
    }
}

