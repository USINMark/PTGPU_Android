
typedef struct TreeNode {
	int min;
	int max;

	int nShape;
	int leaf;
	
	int nLeft, nRight, nParent;

	Bound bound;
	float cost;
	float area;		
} TreeNode;

typedef struct {
    unsigned char partition;
    int left;	
    //TreeNode *parent;
	int nParent;
}PartitionEntry;