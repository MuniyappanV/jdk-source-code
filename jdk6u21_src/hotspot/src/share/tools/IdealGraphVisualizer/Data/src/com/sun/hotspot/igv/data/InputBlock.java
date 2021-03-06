/*
 * Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
package com.sun.hotspot.igv.data;

import java.awt.Rectangle;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 *
 * @author Thomas Wuerthinger
 */
public class InputBlock {

    private List<InputNode> nodes;
    private List<String> successorNames;
    private String name;
    private InputGraph graph;
    private Rectangle bounds;
    private Set<InputBlock> successors;
    private Set<InputBlock> predecessors;
    private Set<InputBlockEdge> inputs;
    private Set<InputBlockEdge> outputs;

    public InputBlock(InputGraph graph, String name) {
        this.graph = graph;
        this.name = name;
        nodes = new ArrayList<InputNode>();
        successorNames = new ArrayList<String>();
        successors = new HashSet<InputBlock>();
        predecessors = new HashSet<InputBlock>();
        inputs = new HashSet<InputBlockEdge>();
        outputs = new HashSet<InputBlockEdge>();
    }

    public void removeSuccessor(InputBlock b) {
        if (successors.contains(b)) {
            successors.remove(b);
            b.predecessors.remove(this);
            InputBlockEdge e = new InputBlockEdge(this, b);
            assert outputs.contains(e);
            outputs.remove(e);
            assert b.inputs.contains(e);
            b.inputs.remove(e);
        }
    }

    public String getName() {
        return name;
    }

    public void setName(String s) {
        name = s;
    }

    public List<InputNode> getNodes() {
        return Collections.unmodifiableList(nodes);
    }

    public void addNode(int id) {
        InputNode n = graph.getNode(id);
        assert n != null;
        graph.setBlock(n, this);
        addNode(graph.getNode(id));
    }

    public void addNode(InputNode node) {
        assert !nodes.contains(node);
        nodes.add(node);
    }

    public Set<InputBlock> getPredecessors() {
        return Collections.unmodifiableSet(predecessors);
    }

    public Set<InputBlock> getSuccessors() {
        return Collections.unmodifiableSet(successors);
    }

    public Set<InputBlockEdge> getInputs() {
        return Collections.unmodifiableSet(inputs);
    }

    public Set<InputBlockEdge> getOutputs() {
        return Collections.unmodifiableSet(outputs);
    }

    // resolveBlockLinks must be called afterwards
    public void addSuccessor(String name) {
        successorNames.add(name);
    }

    public void resolveBlockLinks() {
        for (String s : successorNames) {
            InputBlock b = graph.getBlock(s);
            addSuccessor(b);
        }

        successorNames.clear();
    }

    public void addSuccessor(InputBlock b) {
        if (!successors.contains(b)) {
            successors.add(b);
            b.predecessors.add(this);
            InputBlockEdge e = new InputBlockEdge(this, b);
            outputs.add(e);
            b.inputs.add(e);
        }
    }

    @Override
    public String toString() {
        return this.getName();
    }
}
