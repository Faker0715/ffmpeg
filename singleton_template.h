#pragma once

#include<stddef.h>
template <typename T>
class SingletonFF {
public:
	static T *instance();
	static void destroy();
private:
    SingletonFF();
	~SingletonFF();
private:
	static T *_instance;
};

template <typename T>
T *SingletonFF<T>::_instance = NULL;

template <typename T>
T *SingletonFF<T>::instance() {
	if (_instance == NULL) {
		_instance = new T;
	}

	return _instance;
}

template <typename T>
void SingletonFF<T>::destroy() {
	if (_instance != NULL)
		delete _instance;
}

template <typename T>
SingletonFF<T>::SingletonFF() {
}

template <typename T>
SingletonFF<T>::~SingletonFF() {
}
