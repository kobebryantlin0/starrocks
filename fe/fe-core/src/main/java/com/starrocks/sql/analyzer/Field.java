// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.
package com.starrocks.sql.analyzer;

import com.starrocks.analysis.Expr;
import com.starrocks.analysis.SlotRef;
import com.starrocks.analysis.TableName;
import com.starrocks.catalog.Type;

public class Field {
    private final String name;
    private Type type;
    //shadow column is not visible, eg. schema change column and materialized column
    private final boolean visible;

    /**
     * TableName of field
     * relationAlias is origin table which table name is explicit, such as t0.a
     * Field come from scope is resolved by scope relation alias,
     * such as subquery alias and table relation name
     */
    private final TableName relationAlias;
    private final Expr originExpression;

    public Field(String name, Type type, TableName relationAlias, Expr originExpression) {
        this(name, type, relationAlias, originExpression, true);
    }

    public Field(String name, Type type, TableName relationAlias, Expr originExpression, boolean visible) {
        this.name = name;
        this.type = type;
        this.relationAlias = relationAlias;
        this.originExpression = originExpression;
        this.visible = visible;
    }

    public String getName() {
        return name;
    }

    public TableName getRelationAlias() {
        return relationAlias;
    }

    public Expr getOriginExpression() {
        return originExpression;
    }

    public Type getType() {
        return type;
    }

    public void setType(Type type) {
        this.type = type;
    }

    public boolean isVisible() {
        return visible;
    }

    public boolean canResolve(SlotRef expr) {
        TableName tableName = expr.getTblNameWithoutAnalyzed();
        if (tableName != null) {
            if (relationAlias == null) {
                return false;
            }
            return matchesPrefix(expr.getTblNameWithoutAnalyzed()) && expr.getColumnName().equalsIgnoreCase(this.name);
        } else {
            return expr.getColumnName().equalsIgnoreCase(this.name);
        }
    }

<<<<<<< HEAD
    public boolean matchesPrefix(TableName prefix) {
        if (relationAlias != null) {
            return relationAlias.getTbl().equals(prefix.getTbl());
=======
    private boolean tryToParseAsStructType(SlotRef slotRef) {
        QualifiedName qualifiedName = slotRef.getQualifiedName();
        tmpUsedStructFieldPos.clear();

        if (qualifiedName == null) {
            return slotRef.getColumnName().equalsIgnoreCase(this.name);
        }

        if (relationAlias == null) {
            return false;
        }

        // Generate current field's full qualified name.
        // fieldFullQualifiedName: [CatalogName, DatabaseName, TableName, ColumnName]
        String[] fieldFullQualifiedName = new String[] {
                relationAlias.getCatalog(),
                relationAlias.getDb(),
                relationAlias.getTbl(),
                name
        };

        // First start matching from CatalogName, if it fails, then start matching from DatabaseName, and so on.
        for (int i = 0; i < 4; i++) {
            if (tryToMatch(fieldFullQualifiedName, i, qualifiedName)) {
                return true;
            }
        }
        return false;
    }

    private boolean tryToMatch(String[] fieldFullQualifiedName, int index, QualifiedName qualifiedName) {
        String[] slotRefPartsArray = qualifiedName.getParts().toArray(new String[0]);
        int matchIndex = 0;
        // i = 0 means match from catalog name,
        // i = 1, match from database name,
        // i = 2, match from table name, only table name is case-sensitive,
        // i = 3, match from column name.
        for (; index < 4 && matchIndex < slotRefPartsArray.length; index++) {
            if (fieldFullQualifiedName[index] == null) {
                return false;
            }

            String part = slotRefPartsArray[matchIndex++];
            String comparedPart = fieldFullQualifiedName[index];
            // Only table name is case-sensitive, we will convert other parts to lower case.
            if (index != 2) {
                part = part.toLowerCase();
                comparedPart = comparedPart.toLowerCase();
            }
            if (!part.equals(comparedPart)) {
                return false;
            }
        }

        if (index < 4) {
            // Not match to col name, return false directly.
            return false;
        }

        // matchIndex reach the end of slotRefPartsArray, means this SlotRef matched all.
        if (matchIndex == slotRefPartsArray.length) {
            return true;
        }

        // matchIndex not reach end of slotRefPartsArray, it must be StructType.
        Type tmpType = type;
        for (; matchIndex < slotRefPartsArray.length; matchIndex++) {
            if (!tmpType.isStructType()) {
                return false;
            }
            StructField structField = ((StructType) tmpType).getField(slotRefPartsArray[matchIndex]);
            if (structField == null) {
                return false;
            }
            // Record the struct field position that matches successfully.
            tmpUsedStructFieldPos.add(structField.getPosition());
            tmpType = structField.getType();
        }
        return true;
    }

    public List<Integer> getTmpUsedStructFieldPos() {
        return tmpUsedStructFieldPos;
    }

    public boolean matchesPrefix(TableName tableName) {
        if (tableName.getCatalog() != null && !tableName.getCatalog().equals(relationAlias.getCatalog())) {
            return false;
>>>>>>> 1427b8a4b ([BugFix] Fix column name resolved ignore resolve db name (#13504))
        }

        if (tableName.getDb() != null && !tableName.getDb().equals(relationAlias.getDb())) {
            return false;
        }

        return tableName.getTbl().equals(relationAlias.getTbl());
    }

    @Override
    public String toString() {
        StringBuilder result = new StringBuilder();
        if (name == null) {
            result.append("<anonymous>");
        } else {
            result.append(name);
        }
        result.append(":").append(type);
        return result.toString();
    }
}