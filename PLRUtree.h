#pragma once
class PLRUtree
{
public:
	unsigned size;
	PLRUtree(unsigned size) :size(size){}
	~PLRUtree();
	unsigned addresses[16];
	bool binaryTree[16];

	unsigned getOverwriteTarget()
	{
		int s = size / 2;
		int t = s;
		while (s!=1)
		{
			s /= 2;
			if (binaryTree[t])
				t += s;
			else
				t -= s;
		}
		if (binaryTree[t])
			return t;
		else
			return t - 1;
	}


	void setPath(unsigned element)
	{
		int s = size / 2;
		int t = s;
		while (s != 1)
		{
			s /= 2;
			binaryTree[t] = element < t;
			if (!binaryTree[t])
				t += s;
			else
				t -= s;
		}

	}

};

