namespace Reference {

void* build_context();
void  destroy_context(void*);

ColumnarTable execute(const Plan& plan, void* context);

} // namespace Reference