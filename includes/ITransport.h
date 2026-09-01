class ITransport {
    public:
        virtual void Initialize() = 0;
        virtual int Send() = 0;
        virtual int Recv() = 0;
};
